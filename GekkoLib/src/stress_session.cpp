#include "session/stress_session.h"
#include <cassert>
#include <cstring>


Gekko::StressSession::StressSession()
{
    _config = GekkoConfig();
    _sync = SyncSystem();
    _check_distance = 0;
}

void Gekko::StressSession::Init(GekkoConfig* config)
{
    std::memcpy(&_config, config, sizeof(GekkoConfig));

    // check distance
    _check_distance = _config.check_distance;

    // setup input buffer for the players
    _sync.Init(_config.num_players, _config.input_size);

    // setup game event system
    _game_events.Init(_config.input_size * _config.num_players);

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

i32 Gekko::StressSession::AddActor(GekkoPlayerType type, GekkoNetAddress* addr)
{
    if (type != GekkoLocalPlayer) return -1;

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
    _game_events.Clear();
    _session_events.Reset();
    _game_events.Reset();

    // store the frames that went by for the replay
    UpdateRecording();

    Frame current = _sync.GetCurrentFrame();
    if (_check_distance > 0 && current > _check_distance) {
        // once whe have gone far enough forward start rollback back and comparing checksums
        for (Frame check_frame = current - _check_distance; check_frame < current; check_frame++) {
            CheckForDesyncs(check_frame);
        }
        // rollback according to check distance
        HandleRollback();
    }

    if (current - 1 == GameInput::NULL_FRAME - 1) {
        // initial gamestate save
        _game_events.AddSaveEvent(_sync, _storage);
    }

    // advance the session forward
    if (_game_events.AddAdvanceEvent(_sync, false)) {
        _game_events.AddSaveEvent(_sync, _storage);
        _sync.IncrementFrame();
    }

    *count = _game_events.Count();
    return _game_events.Data();
}

GekkoSessionEvent** Gekko::StressSession::Events(i32* count)
{
    *count = (i32)_session_events.GetRecentEvents().size();
    return _session_events.GetRecentEvents().data();
}

bool Gekko::StressSession::StartRecording(bool save_initial_state, bool disable_compression)
{
    return _replay.StartRecording(_config, _sync.GetCurrentFrame(), save_initial_state, disable_compression);
}

const u8* Gekko::StressSession::StopRecording(u32& length)
{
    FlushRecording();

    return _replay.StopRecording(length);
}

const u8* Gekko::StressSession::PeekRecording(u32& length)
{
    FlushRecording();

    return _replay.PeekRecording(length);
}

void Gekko::StressSession::FlushRecording()
{
    // the game handled the events of the last update by now, keep what they confirmed.
    if (_replay.IsRecording() && !_replay.NeedsState()) {
        _replay.RecordInputs(_sync);
        _replay.RecordChecksums(_storage, _sync.GetCurrentFrame() - 1);
    }
}

bool Gekko::StressSession::RecordChecksums(u32 interval)
{
    return _replay.SetChecksumInterval(interval);
}

bool Gekko::StressSession::SetReplayUserData(const u8* data, u32 length)
{
    return _replay.SetUserData(data, length);
}

void Gekko::StressSession::UpdateRecording()
{
    if (!_replay.IsRecording()) {
        return;
    }

    if (_replay.NeedsState()) {
        _game_events.AddStateSaveEvent(_sync.GetCurrentFrame() - 1, _replay.PendingState());
    }

    _replay.RecordInputs(_sync);

    // a local session has no unconfirmed frames, every frame it saved is final.
    _replay.RecordChecksums(_storage, _sync.GetCurrentFrame() - 1);
}

void Gekko::StressSession::HandleRollback()
{
    const Frame current = _sync.GetCurrentFrame();
    const Frame past = current - _check_distance;

    // load the sync frame
    _sync.SetCurrentFrame(past);
    _game_events.AddLoadEvent(_sync, _storage);
    _sync.IncrementFrame();

    for (Frame frame = past + 1; frame < current; frame++) {
        _game_events.AddAdvanceEvent(_sync, true);
        _game_events.AddSaveEvent(_sync, _storage);
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
