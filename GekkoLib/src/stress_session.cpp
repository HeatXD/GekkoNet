#include "stress_session.h"


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

    // setup input buffer for the players
    _sync.Init(_config.num_players, _config.input_size);

    //setup game event system
    _game_event_buffer.Init(_config.input_size * _config.num_players);

    // setup state storage
    _storage.Init(_config.input_prediction_window, _config.state_size, false);

    // check distance
    _check_distance = _config.check_distance;
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

    }

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
