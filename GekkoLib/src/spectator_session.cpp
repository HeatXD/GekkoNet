#include "session/spectator_session.h"

#include <algorithm>
#include <cstring>

Gekko::SpectatorSession::SpectatorSession()
{
	_host = nullptr;
	_started = false;
    _delay_spectator = false;
    _spectator_state_pending = false;
    _spectator_state_frame = GameInput::NULL_FRAME;
    _checksum_pending = false;
    _last_saved_frame = GameInput::NULL_FRAME - 1;
    _config = GekkoConfig();
}

void Gekko::SpectatorSession::Init(GekkoConfig* config)
{
    _host = nullptr;

    _started = false;

    // get given configs
    std::memcpy(&_config, config, sizeof(GekkoConfig));

    // setup input buffer for the players (room for the spectator delay and for falling behind)
    _sync.Init(_config.num_players, _config.input_size, InputBufferSize());

    // setup message system.
    _msg.Init(_config.num_players, _config.input_size, _config.state_size, true);

    // setup game event system
    _game_events.Init(_config.input_size * _config.num_players);

    _checksum_state.state = std::make_unique<u8[]>(_config.state_size);
    _checksum_state.state_len = _config.state_size;
    _checksum_pending = false;

    // start paused so the buffer fills before playback begins
    _delay_spectator = (_config.spectator_delay > 0);
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
    if (type != GekkoRemotePlayer) {
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

bool Gekko::SpectatorSession::DisconnectActor(i32 actor)
{
    // disconnecting the host stops the spectating session.
    if (!_msg.DisconnectActor(actor)) {
        return false;
    }

    // flush right away so the disconnect gets sent even
    // when the session isnt updated after this call.
    if (_host) {
        _msg.SendPendingOutput(_host);
    }

    return true;
}

void Gekko::SpectatorSession::SetDisconnectTimeout(u32 timeout)
{
    _msg.SetDisconnectTimeout(timeout);
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

        if (_spectator_state_pending) {
            _game_events.AddStateLoadEvent(
                _spectator_state_frame,
                _spectator_state.data(),
                (u32)_spectator_state.size()
            );
            _spectator_state_pending = false;
        }

        // store the frames that went by for the replay
        UpdateRecording();

        // spectator session buffer
        if (ShouldDelaySpectator()) {
            *count = _game_events.Count();
            return _game_events.Data();
        }

        // then advance the session
        if (_game_events.AddAdvanceEvent(_sync, false)) {
            const Frame frame = _sync.GetCurrentFrame();
            if (_replay.WantsChecksum(frame)) {
                _game_events.AddStateSaveEvent(frame, &_checksum_state);
                _checksum_pending = true;
            }
            _sync.IncrementFrame();
        }
    }

    *count = _game_events.Count();
    return _game_events.Data();
}

u32 Gekko::SpectatorSession::InputBufferSize() const
{
    return InputBuffer::DEFAULT_BUFF_SIZE + _config.spectator_delay + CATCH_UP_FRAMES;
}

i32 Gekko::SpectatorSession::SpectatorBufferedFrames()
{
    const Frame received = _sync.GetMinReceivedFrame();
    if (received == GameInput::NULL_FRAME) {
        return 0;
    }

    return std::max(0, (i32)(received - _sync.GetCurrentFrame() + 1));
}

GekkoSessionEvent** Gekko::SpectatorSession::Events(i32* count)
{
    *count = (i32)_msg.session_events.GetRecentEvents().size();
    return _msg.session_events.GetRecentEvents().data();
}

void Gekko::SpectatorSession::NetworkStats(i32 player, GekkoNetworkStats* stats)
{
    for (auto& actor : _msg.remotes) {
        if (actor->handle == player) {
            actor->stats.UpdateBandwidth();
            stats->kb_sent = actor->stats.kb_sent_per_sec;
            stats->kb_received = actor->stats.kb_received_per_sec;
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

bool Gekko::SpectatorSession::StartRecording(bool save_initial_state, bool disable_compression)
{
    return _replay.StartRecording(_config, _sync.GetCurrentFrame(), save_initial_state, disable_compression);
}

const u8* Gekko::SpectatorSession::StopRecording(u32& length)
{
    FlushRecording();

    return _replay.StopRecording(length);
}

const u8* Gekko::SpectatorSession::PeekRecording(u32& length)
{
    FlushRecording();

    return _replay.PeekRecording(length);
}

void Gekko::SpectatorSession::FlushRecording()
{
    // the game handled the events of the last update by now, keep what they confirmed.
    if (_replay.IsRecording() && !_replay.NeedsState()) {
        _replay.RecordInputs(_sync);
        RecordPendingChecksum();
    }
}

bool Gekko::SpectatorSession::RecordChecksums(u32 interval)
{
    return _replay.SetChecksumInterval(interval);
}

bool Gekko::SpectatorSession::SetReplayUserData(const u8* data, u32 length)
{
    return _replay.SetUserData(data, length);
}

void Gekko::SpectatorSession::RecordPendingChecksum()
{
    if (!_checksum_pending) {
        return;
    }

    _checksum_pending = false;

    _replay.RecordChecksum(_checksum_state.frame, _checksum_state.checksum);
}

void Gekko::SpectatorSession::UpdateRecording()
{
    if (!_replay.IsRecording()) {
        return;
    }

    if (_replay.NeedsState()) {
        _game_events.AddStateSaveEvent(_sync.GetCurrentFrame() - 1, _replay.PendingState());
    }

    _replay.RecordInputs(_sync);

    // spectators only play confirmed inputs, so the frame saved last update is final.
    RecordPendingChecksum();
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

    _msg.CheckStatusActors();

    Frame state_frame = GameInput::NULL_FRAME;
    std::vector<u8> state;
    if (_msg.TakeSpectatorState(state_frame, state)) {
        // The snapshot contains the state after this frame, so playback starts
        // with the next input frame.
        _sync.Init(_config.num_players, _config.input_size, InputBufferSize());
        _sync.SetCurrentFrame(state_frame + 1);
        _sync.SetLastReceivedFrame(state_frame);

        _spectator_state_frame = state_frame;
        _spectator_state = std::move(state);
        _spectator_state_pending = true;
        _delay_spectator = (_config.spectator_delay > 0);
    }

	// handle received inputs
	HandleReceivedInputs();

	// send network health update
	_msg.SendNetworkHealth();

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
        if (_config.spectator_delay > 0) {
            _msg.session_events.AddSpectatorPausedEvent();
        }
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
                    _msg.SendInputAck(handle, i, 0);
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

    // Re-pause only when the buffer is completely exhausted
    if (diff == 0) {
        _delay_spectator = true;
        _msg.session_events.AddSpectatorPausedEvent();
        return true;
    }

    return false;
}
