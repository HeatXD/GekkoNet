#include "session.h"


Gekko::SpectatorSession::SpectatorSession()
{
	_host = nullptr;
	_started = false;
    _delay_spectator = false;
    _last_saved_frame = GameInput::NULL_FRAME - 1;
    _config = GekkoConfig();
}

void Gekko::SpectatorSession::Init(GekkoConfig* config)
{
    _host = nullptr;

    _started = false;

    // get given configs
    std::memcpy(&_config, config, sizeof(GekkoConfig));

    // setup input buffer for the players (add size for spectator delay)
    u32 buffer_size = InputBuffer::DEFAULT_BUFF_SIZE + _config.spectator_delay;
    _sync.Init(_config.num_players, _config.input_size, buffer_size);

    // setup message system.
    _msg.Init(_config.num_players, _config.input_size);

    // setup game event system
    _game_events.Init(_config.input_size * _config.num_players);
}

void Gekko::SpectatorSession::SetLocalDelay(i32 player, u8 delay)
{
    // no-op: spectators don't have local players
}

void Gekko::SpectatorSession::SetNetAdapter(GekkoNetAdapter* adapter)
{
    _host = adapter;
}

i32 Gekko::SpectatorSession::AddActor(GekkoPlayerType type, GekkoNetAddress* addr)
{
    const i32 ERR = -1;
    std::unique_ptr<NetAddress> address;

    // only accept a single remote player (the host)
    if (type != RemotePlayer) {
        return ERR;
    }

    if (addr == nullptr) {
        return ERR;
    }

    if (!_msg.remotes.empty()) {
        return ERR;
    }

    address = std::make_unique<NetAddress>(addr->data, addr->size);

    u32 new_handle = (u32)_msg.remotes.size();
    _msg.remotes.push_back(std::make_unique<Player>(new_handle, type, address.get()));

    return new_handle;
}

void Gekko::SpectatorSession::AddLocalInput(i32 player, void* input)
{
    // no-op: spectators don't add local input
}

GekkoGameEvent** Gekko::SpectatorSession::UpdateSession(i32* count)
{
    // reset session events
    _msg.session_events.Reset();

    // connection handling
    Poll();

    // clear GameEvents
    _game_events.Clear();

    // gameplay
    if (AllActorsValid()) {
        // reset the game event buffer before doing anything else
        _game_events.Reset();

        // spectator session buffer
        if (ShouldDelaySpectator()) {
            *count = _game_events.Count();
            return _game_events.Data();
        }

        // then advance the session
        if (_game_events.AddAdvanceEvent(_sync, false)) {
            _sync.IncrementFrame();
        }
    }

    *count = _game_events.Count();
    return _game_events.Data();
}

GekkoSessionEvent** Gekko::SpectatorSession::Events(i32* count)
{
    *count = (i32)_msg.session_events.GetRecentEvents().size();
    return _msg.session_events.GetRecentEvents().data();
}

f32 Gekko::SpectatorSession::FramesAhead()
{
    return 0.f;
}

void Gekko::SpectatorSession::NetworkStats(i32 player, GekkoNetworkStats* stats)
{
    for (auto& actor : _msg.remotes) {
        if (actor->handle == player) {
            stats->last_ping = actor->stats.LastRTT();
            stats->jitter = actor->stats.CalculateJitter();
            stats->avg_ping = actor->stats.CalculateAvgRTT();
            return;
        }
    }
}

void Gekko::SpectatorSession::NetworkPoll()
{
    Poll();
}

void Gekko::SpectatorSession::Poll()
{
	// return if no host is defined.
    if (!_host) {
        return;
    }

    // fetch data from network
    int length = 0;
	auto data = _host->receive_data(&length);

    // process the data we received
    _msg.HandleData(_host, data, length);

	// handle received inputs
	HandleReceivedInputs();

	// now send data
	_msg.SendPendingOutput(_host);
}

bool Gekko::SpectatorSession::AllActorsValid()
{
	if (!_started) {
		if (!_msg.CheckStatusActors()) {
			return false;
		}

		// if none returned that the session is ready!
        _msg.session_events.AddSessionStartedEvent();

        _started = true;

		return true;
	}

	return true;
}

void Gekko::SpectatorSession::HandleReceivedInputs()
{
    for (auto& remote : _msg.remotes) {
        if (remote->GetStatus() != Connected) continue;

        // spectators receive combined inputs for ALL players from the host
        std::vector<Handle> handles;
        for (u32 i = 0; i < _config.num_players; i++) {
            handles.push_back(i);
        }

        for (u32 i = 0; i < handles.size(); i++) {
            auto handle = handles[i];
            const Frame last_recv = _sync.GetLastReceivedFrom(handle) + 1;
            const Frame last_added = _msg.GetLastAddedInputFrom(handle);

            auto& input_q = _msg.GetNetPlayerQueue(handle);
            const Frame min_frame = last_added - (i32)input_q.size() + 1;
            for (int i = last_recv; i <= last_added; i++) {
                if (i >= min_frame) {
                    int current_idx = i - min_frame;
                    u8* input = input_q[current_idx].get();
                    _sync.AddRemoteInput(handle, input, i);
                    _msg.SendInputAck(handle, i);
                }
            }
        }
    }
}

bool Gekko::SpectatorSession::ShouldDelaySpectator()
{
    if (_config.spectator_delay == 0) {
        return false;
    }

    const u32 delay = _config.spectator_delay;
    const Frame current = _sync.GetCurrentFrame();
    const Frame min = _sync.GetMinReceivedFrame();
    const u32 diff = std::abs(min - current);

    if (_delay_spectator) {
        if (diff >= delay) {
            _delay_spectator = false;
            _msg.session_events.AddSpectatorUnpausedEvent();
            return false;
        }
        return true;
    }

    // check every 600 frames (10 seconds at 60 fps) whether it should add delay.
    if (current % 600 == 0) {
        _delay_spectator = diff < delay;

        if (_delay_spectator) {
            _msg.session_events.AddSpectatorPausedEvent();
            return true;
        }
    }

    return false;
}
