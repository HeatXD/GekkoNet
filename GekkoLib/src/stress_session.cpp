#include "stress_session.h"
#include <cassert>


Gekko::StressSession::StressSession()
{
    _config = GekkoConfig();
    _session_events = SessionEventSystem();
    _storage = StateStorage();
    _sync = SyncSystem();
    _game_event_buffer = GameEventBuffer();
    _check_distance = 0;
}

void Gekko::StressSession::Init(GekkoConfig* config)
{
    std::memcpy(&_config, config, sizeof(GekkoConfig));

    // check distance
    _check_distance = _config.check_distance;

    // setup input buffer for the players
    _sync.Init(_config.num_players, _config.input_size);

    //setup game event system
    _game_event_buffer.Init(_config.input_size * _config.num_players);

    // setup state storage
    _storage.Init(_check_distance, _config.state_size, false);

    // setup checksum history for comparisons
    _checksum_history.clear();
}

void Gekko::StressSession::SetLocalDelay(i32 player, u8 delay)
{
    for (u32 i = 0; i < _locals.size(); i++) {
        if (_locals[i].handle == player) {
            _sync.SetLocalDelay(player, delay);
        }
    }
}

void Gekko::StressSession::SetNetAdapter(GekkoNetAdapter* adapter)
{
   // no adapters used in stress sessions.
}

i32 Gekko::StressSession::AddActor(GekkoPlayerType type, GekkoNetAddress* addr)
{
    if (type != LocalPlayer) return -1;

    u32 new_handle = (u32)_locals.size();
    std::unique_ptr<NetAddress> address;

    if (addr) {
        address = std::make_unique<NetAddress>(addr->data, addr->size);
    }

    _locals.push_back(Player(new_handle, type, address.get()));

    return new_handle;
}

void Gekko::StressSession::AddLocalInput(i32 player, void* input)
{
    u8* inp = (u8*)input;

    for (u32 i = 0; i < _locals.size(); i++) {
        if (_locals[i].handle == player) {
            _sync.AddLocalInput(player, inp);
            break;
        }
    }
}

GekkoGameEvent** Gekko::StressSession::UpdateSession(i32* count)
{
    _current_game_events.clear();

    _session_events.Reset();
    _game_event_buffer.Reset();

    Frame current = _sync.GetCurrentFrame();
    if (_check_distance > 0 && current > _check_distance) {
        // once whe have gone far enough forward start rollback back and comparing checksums
        for (Frame check_frame = current - _check_distance; check_frame < current; check_frame++) {
            CheckForDesyncs(check_frame);
        }
        // rollback according to check distance
        HandleRollback(_current_game_events);
    }

    if (current - 1 == GameInput::NULL_FRAME - 1) {
        // initial gamestate save
        AddSaveEvent(_current_game_events);
    }

    // advance the session forward
    if (AddAdvanceEvent(_current_game_events, false)) {
        AddSaveEvent(_current_game_events);
        _sync.IncrementFrame();
    }

    *count = (i32)_current_game_events.size();
    return _current_game_events.data();
}

GekkoSessionEvent** Gekko::StressSession::Events(i32* count)
{
    *count = (i32)_session_events.GetRecentEvents().size();
    return _session_events.GetRecentEvents().data();
}

f32 Gekko::StressSession::FramesAhead()
{
    return 0.f;
}

void Gekko::StressSession::NetworkStats(i32 player, GekkoNetworkStats* stats)
{
    // no stats for now.
}

void Gekko::StressSession::NetworkPoll()
{
    // stress sessions are local only
}

void Gekko::StressSession::HandleRollback(std::vector<GekkoGameEvent*>& ev)
{
    const Frame current = _sync.GetCurrentFrame();
    const Frame past = current - _check_distance;

    // load the sync frame
    _sync.SetCurrentFrame(past);
    AddLoadEvent(ev);
    _sync.IncrementFrame();

    for (Frame frame = past + 1; frame < current; frame++) {
        AddAdvanceEvent(ev, true);
        AddSaveEvent(ev);
        _sync.IncrementFrame();
    }

    // make sure that we are back where we started.
    assert(_sync.GetCurrentFrame() == current);
}

void Gekko::StressSession::CheckForDesyncs(Frame check_frame)
{
    const Frame oldest = check_frame - _check_distance;

    // clear old checksum frames
    _checksum_history.erase(_checksum_history.begin(), _checksum_history.lower_bound(oldest));

    const StateEntry* state = _storage.GetState(check_frame);
    // check if there is a valid entry in the state storage
    if (state->frame == check_frame) {
        if (_checksum_history.count(check_frame)) {
            // found in the history! compare checksums and send event if incorrect.
            u32 recorded_checksum = _checksum_history[check_frame];
            if (state->checksum != recorded_checksum) {
                _session_events.AddDesyncDetectedEvent(check_frame, 0, state->checksum, recorded_checksum);
            }
        } else {
            // not in the history? then add it.
            _checksum_history.emplace(check_frame, state->checksum);
        }
    } 
}

bool Gekko::StressSession::AddAdvanceEvent(std::vector<GekkoGameEvent*>& ev, bool rolling_back)
{
    Frame frame = GameInput::NULL_FRAME;
    std::unique_ptr<u8[]> inputs;
    if (!_sync.GetCurrentInputs(inputs, frame)) {
        return false;
    }

    ev.push_back(_game_event_buffer.GetEvent(true));

    auto event = ev.back();

    event->type = AdvanceEvent;
    event->data.adv.frame = frame;
    event->data.adv.rolling_back = rolling_back;

    if (event->data.adv.inputs) {
        std::memcpy(event->data.adv.inputs, inputs.get(), event->data.adv.input_len);
    }
    return true;
}

void Gekko::StressSession::AddSaveEvent(std::vector<GekkoGameEvent*>& ev)
{
    const Frame frame_to_save = _sync.GetCurrentFrame();

    auto state = _storage.GetState(frame_to_save);

    state->frame = frame_to_save;

    ev.push_back(_game_event_buffer.GetEvent(false));

    auto event = ev.back();
    event->type = SaveEvent;

    event->data.save.frame = frame_to_save;
    event->data.save.state = state->state.get();
    event->data.save.checksum = &state->checksum;
    event->data.save.state_len = &state->state_len;
}

void Gekko::StressSession::AddLoadEvent(std::vector<GekkoGameEvent*>& ev)
{
    const Frame frame_to_load = _sync.GetCurrentFrame();

    auto state = _storage.GetState(frame_to_load);

    ev.push_back(_game_event_buffer.GetEvent(false));

    auto event = ev.back();
    event->type = LoadEvent;

    event->data.load.frame = frame_to_load;
    event->data.load.state = state->state.get();
    event->data.load.state_len = state->state_len;
}
