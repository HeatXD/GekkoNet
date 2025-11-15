#include "gekko.h"
#include "backend.h"

#include <cassert>

GekkoSession::~GekkoSession() = default;

Gekko::Session::Session()
{
	_host = nullptr;
	_started = false;
    _delay_spectator = false;
    _last_saved_frame = GameInput::NULL_FRAME - 1;
	_disconnected_input = nullptr;
    _last_sent_healthcheck = GameInput::NULL_FRAME;
    _config = GekkoConfig();
}

void Gekko::Session::Init(GekkoConfig* config)
{
    _host = nullptr;

    _started = false;

    // get given configs
    std::memcpy(&_config, config, sizeof(GekkoConfig));

    // setup input buffer for the players
    _sync.Init(_config.num_players, _config.input_size);

    // setup message system.
    _msg.Init(_config.num_players, _config.input_size);

    //setup game event system
    _game_event_buffer.Init(_config.input_size * _config.num_players);

    // setup state storage
    _storage.Init(_config.input_prediction_window, _config.state_size, _config.limited_saving);

    // setup disconnected input for disconnected player within the session
    _disconnected_input = std::make_unique<u8[]>(_config.input_size);
    std::memset(_disconnected_input.get(), 0, _config.input_size);

    // we only detect desyncs whenever we are not limited saving for now.
    _config.desync_detection = _config.limited_saving ? false : _config.desync_detection;
}

void Gekko::Session::SetLocalDelay(i32 player, u8 delay)
{
    for (u32 i = 0; i < _msg.locals.size(); i++) {
        if (_msg.locals[i]->handle == player) {
            _sync.SetLocalDelay(player, delay);
        }
    }
}

void Gekko::Session::SetNetAdapter(GekkoNetAdapter* adapter)
{
    _host = adapter;
}

i32 Gekko::Session::AddActor(GekkoPlayerType type, GekkoNetAddress* addr)
{
    const i32 ERR = -1;
    std::unique_ptr<NetAddress> address;

    if (addr) {
        address = std::make_unique<NetAddress>(addr->data, addr->size);
    }

    if (type == Spectator) {
        if (_msg.spectators.size() >= _config.max_spectators) {
            return ERR;
        }

        u32 new_handle = _config.num_players + (u32)_msg.spectators.size();
        _msg.spectators.push_back(std::make_unique<Player>(new_handle, type, address.get()));

        return new_handle;
    }
    else {
        if (_started || _msg.locals.size() + _msg.remotes.size() >= _config.num_players) {
            return ERR;
        }

        u32 new_handle = (u32)(_msg.locals.size() + _msg.remotes.size());

        if (type == LocalPlayer) {
            _msg.locals.push_back(std::make_unique<Player>(new_handle, type, address.get()));
        }
        else {
            // require an address when specifing a remote player
            if (addr == nullptr) {
                return ERR;
            }

            _msg.remotes.push_back(std::make_unique<Player>(new_handle, type, address.get()));
            _sync.SetInputPredictionWindow(new_handle, _config.input_prediction_window);
        }

        return new_handle;
    }
}

void Gekko::Session::AddLocalInput(i32 player, void* input)
{
    u8* inp = (u8*)input;

    for (u32 i = 0; i < _msg.locals.size(); i++) {
        if (_msg.locals[i]->handle == player) {
            _sync.AddLocalInput(player, inp);
            break;
        }
    }
}

GekkoGameEvent** Gekko::Session::UpdateSession(i32* count)
{
    // reset session events
    _msg.session_events.Reset();

    // connection Handling
    Poll();

    // clear GameEvents
    _current_game_events.clear();

    // gameplay
    if (AllActorsValid()) {
        // reset the game event buffer before doing anything else
        _game_event_buffer.Reset();

        // add inputs so we can continue the session.
        AddDisconnectedPlayerInputs();

        // check if we need to rollback
        HandleRollback(_current_game_events);

        // check if we need to save the confirmed frame
        HandleSavingConfirmedFrame(_current_game_events);

        // spectator session buffer
        if (ShouldDelaySpectator()) {
            *count = (i32)_current_game_events.size();
            return _current_game_events.data();
        }

        // send a healthcheck if applicable
        SendSessionHealthCheck();

        // check if the session is still doing alright.
        SessionIntegrityCheck();

        // then advance the session
        if (AddAdvanceEvent(_current_game_events, false)) {
            if (!_config.limited_saving ||
                (IsSpectating() || IsPlayingLocally()) &&
                _sync.GetCurrentFrame() % _config.input_prediction_window == 0) {
                AddSaveEvent(_current_game_events);
            }
            _sync.IncrementFrame();
        }
    }

    *count = (i32)_current_game_events.size();
    return _current_game_events.data();
}

GekkoSessionEvent** Gekko::Session::Events(i32* count)
{
    *count = (i32)_msg.session_events.GetRecentEvents().size();
    return _msg.session_events.GetRecentEvents().data();
}

f32 Gekko::Session::FramesAhead()
{
    if (!_started) {
        return 0.f;
    }

	return _msg.history.GetAverageAdvantage();
}

void Gekko::Session::NetworkStats(i32 player, GekkoNetworkStats* stats)
{
    std::vector<std::unique_ptr<Player>>* current = &_msg.remotes;

    for (u32 i = 0; i < 2; i++)
    {
        if (i == 1) {
            current = &_msg.spectators;
        }

        for (auto& actor : *current) {
            if (actor->handle == player) {
                stats->last_ping = actor->stats.LastRTT();
                stats->jitter = actor->stats.CalculateJitter();
                stats->avg_ping = actor->stats.CalculateAvgRTT();
                return;
            }
        }
    }
}

void Gekko::Session::NetworkPoll()
{
    Poll();
}

void Gekko::Session::HandleSavingConfirmedFrame(std::vector<GekkoGameEvent*>& ev)
{
	if (IsLockstepActive() || !_config.limited_saving ||
        IsSpectating() || IsPlayingLocally()) {
		return;
	}

	const Frame confirmed_frame = _sync.GetMinReceivedFrame();
	const Frame current = _sync.GetCurrentFrame();
	const Frame diff = current - (_last_saved_frame + 1);

	if (diff <= _config.input_prediction_window) {
		return;
	}

	assert(_last_saved_frame < confirmed_frame);

	const Frame sync_frame = _last_saved_frame;
	const Frame frame_to_save = std::min(current - 1, confirmed_frame);

	_sync.SetCurrentFrame(sync_frame);
	AddLoadEvent(ev);
	_sync.IncrementFrame();

	for (Frame frame = sync_frame + 1; frame < current; frame++) {
		AddAdvanceEvent(ev, true);
		if (frame == frame_to_save) {
			AddSaveEvent(ev);
		}
		_sync.IncrementFrame();
	}

	// make sure that we are back where we started.
	assert(_sync.GetCurrentFrame() == current);
}

void Gekko::Session::UpdateLocalFrameAdvantage()
{
    if (!_started || IsSpectating()) {
        return;
    }

	const Frame min = _sync.GetMinReceivedFrame();
	const Frame current = _sync.GetCurrentFrame();
    const i32 local_advantage = current - min;

	_msg.history.SetLocalAdvantage(local_advantage);
}

bool Gekko::Session::ShouldDelaySpectator()
{
    if (!IsSpectating() || _config.spectator_delay == 0) {
        return false;
    }

    const u8 delay = std::min(_config.spectator_delay, (u8)(InputBuffer::BUFF_SIZE * 0.75));
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

void Gekko::Session::SendSessionHealthCheck()
{
    if (!_config.desync_detection || IsSpectating()) {
        return;
    }

    const Frame current = _sync.GetCurrentFrame();
    const Frame confirmed = (current - _config.input_prediction_window) - 1;

    if (confirmed <= GameInput::NULL_FRAME) {
        return;
    }

    if (confirmed <= _last_sent_healthcheck) {
        return;
    }

    auto sav = _storage.GetState(confirmed);

    assert(sav->frame == confirmed);

    _last_sent_healthcheck = confirmed;

    _msg.local_health[confirmed] = sav->checksum;

    _msg.SendSessionHealth(confirmed, sav->checksum);

    for (auto iter = _msg.local_health.begin();
        iter != _msg.local_health.end(); ) {
        if (iter->first < (confirmed - 100)) {
            iter = _msg.local_health.erase(iter);
        } else {
            ++iter;
        }
    }
}

void Gekko::Session::SendNetworkHealthCheck()
{
    // we want the session to be synced before trying to determine its network health.
    if (IsSpectating()) {
        return;
    }

    _msg.SendNetworkHealth();
}

void Gekko::Session::SessionIntegrityCheck()
{
    if (!_config.desync_detection || IsSpectating()) {
        return;
    }

    for (auto iter = _msg.local_health.begin();
        iter != _msg.local_health.end(); ) {

        for (auto& player : _msg.remotes) {
            if (player->session_health.count(iter->first)) {
                if (player->session_health[iter->first] != iter->second) {
                    _msg.session_events.AddDesyncDetectedEvent(
                        iter->first,
                        player->handle,
                        iter->second,
                        player->session_health[iter->first]
                    );
                }
                player->session_health.erase(iter->first);
            }
        }

        ++iter;
    }
}

void Gekko::Session::AddDisconnectedPlayerInputs()
{
	for (auto& player : _msg.remotes) {
		if (player->GetStatus() == Disconnected) {
            const Frame last_recv = _sync.GetLastReceivedFrom(player->handle) + 1;
            const Frame current = _sync.GetCurrentFrame();
            for (Frame i = last_recv; i < current; i++) {
                _sync.AddRemoteInput(player->handle, _disconnected_input.get(), i);
            }
		}
	}
}

void Gekko::Session::SendSpectatorInputs()
{
	const Frame current = _msg.GetLastAddedInput(true) + 1;
	const Frame confirmed = _sync.GetMinReceivedFrame();

	std::unique_ptr<u8[]> inputs;
	for (Frame frame = current; frame <= confirmed; frame++) {
		if (!_sync.GetSpectatorInputs(inputs, frame)) {
			break;
		}
		_msg.AddSpectatorInput(frame, inputs.get());
	}
}

void Gekko::Session::HandleRollback(std::vector<GekkoGameEvent*>& ev)
{
	Frame current = _sync.GetCurrentFrame();
	if (_last_saved_frame == GameInput::NULL_FRAME - 1) {
		_sync.SetCurrentFrame(current - 1);
		AddSaveEvent(ev);
		_sync.IncrementFrame();
	}

	if (IsLockstepActive() || IsSpectating() || IsPlayingLocally()) {
        return;
    }

	current = _sync.GetCurrentFrame();
	const Frame min = _sync.GetMinIncorrectFrame();

	// dont allow rollbacks starting before the null frame
    if (min == GameInput::NULL_FRAME) {
        return;
    }

	const Frame sync_frame = _config.limited_saving ? _last_saved_frame : min - 1;
	const Frame frame_to_save = std::min(current - 1, min);

	// load the sync frame
 	_sync.SetCurrentFrame(sync_frame);
	AddLoadEvent(ev);
	_sync.IncrementFrame();

	for (Frame frame = sync_frame + 1; frame < current; frame++) {
        AddAdvanceEvent(ev, true);
		if (!_config.limited_saving || frame == frame_to_save) {
			AddSaveEvent(ev);
		}
		_sync.IncrementFrame();
	}

    // clear the marked mispredictions up to this point in the input buffer
    _sync.ClearIncorrectFramesUpTo(current);

	// make sure that we are back where we started.
	assert(_sync.GetCurrentFrame() == current);
}

bool Gekko::Session::AddAdvanceEvent(std::vector<GekkoGameEvent*>& ev, bool rolling_back)
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

void Gekko::Session::AddSaveEvent(std::vector<GekkoGameEvent*>& ev)
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

	_last_saved_frame = frame_to_save;
}

void Gekko::Session::AddLoadEvent(std::vector<GekkoGameEvent*>& ev)
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

void Gekko::Session::Poll()
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

	// update local frame advantage
	UpdateLocalFrameAdvantage();

	// add local input for the network
	SendLocalInputs();

	// send inputs to spectators
	SendSpectatorInputs();

    // send network health update
    SendNetworkHealthCheck();

	// now send data
	_msg.SendPendingOutput(_host);
}

bool Gekko::Session::AllActorsValid()
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

void Gekko::Session::HandleReceivedInputs()
{
    for (auto& remote : _msg.remotes) {
        if (remote->GetStatus() != Connected) continue;

        std::vector<Handle> handles;

        if (IsSpectating()) {
            for (u32 i = 0; i < _config.num_players; i++) {
                handles.push_back(i);
            }
        } else {
            handles.push_back(remote->handle);
        }

        for (u32 i = 0; i < handles.size(); i++) {
            auto handle = handles[i];
            const Frame last_recv = _sync.GetLastReceivedFrom(handle) + 1;
            const Frame last_added = _msg.GetLastAddedInputFrom(handle);

            assert(last_added - last_recv <= 128); // more then 128 frames behind sounds incorrect.

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

void Gekko::Session::SendLocalInputs()
{
	if (!_msg.locals.empty() && _started) {
		const Frame current = _msg.GetLastAddedInput(false) + 1;
        const Frame delay = GetMinLocalDelay();

		auto input = std::make_unique<u8[]>(_config.input_size);
		for (Frame frame = current; frame <= current + delay; frame++) {
            for (auto& player : _msg.locals) {
                if (!_sync.GetLocalInput(player->handle, input, frame)) {
                    return;
                }
                _msg.AddInput(frame, player->handle, input.get());
            }
		}
	}
}

u8 Gekko::Session::GetMinLocalDelay()
{
	u8 min = UINT8_MAX;
	for (auto& player : _msg.locals) {
		min = std::min(_sync.GetLocalDelay(player->handle), min);
	}
	return min;
}

bool Gekko::Session::IsSpectating()
{
	return _msg.remotes.size() == 1 && _msg.locals.empty();
}


bool Gekko::Session::IsPlayingLocally()
{
	return _msg.remotes.empty() && !_msg.locals.empty();
}

bool Gekko::Session::IsLockstepActive() const
{
    return _config.input_prediction_window == 0;
}
