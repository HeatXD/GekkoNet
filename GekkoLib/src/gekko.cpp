#include "gekko.h"
#include "backend.h"

#include <cassert>

Gekko::Session::Session()
{
	_host = nullptr;
	_started = false;
    _delay_spectator = false;
    _last_saved_frame = GameInput::NULL_FRAME - 1;
	_disconnected_input = nullptr;
    _last_sent_healthcheck = GameInput::NULL_FRAME;
}

void Gekko::Session::SetNetAdapter(NetAdapter* adapter)
{
	_host = adapter;
}

void Gekko::Session::Init(Config& config)
{
	_host = nullptr;

	_started = false;

	// get given configs
    std::memcpy(&_config, &config, sizeof(Config));

	// setup input buffer for the players
	_sync.Init(_config.num_players, _config.input_size);

	// setup message system.
	_msg.Init(_config.input_size);

    //setup game event system
    _game_event_buffer.Init(_config.input_size * _config.num_players);

	// setup state storage
	_storage.Init(_config.input_prediction_window, _config.state_size, _config.limited_saving);

	// setup disconnected input for disconnected player within the session
	_disconnected_input = std::unique_ptr<u8[]>(new u8[_config.input_size]);
	std::memset(_disconnected_input.get(), 0, _config.input_size);

    // we only detect desyncs whenever we are not limited saving for now.
    _config.desync_detection = _config.limited_saving ? false : _config.desync_detection;
}

void Gekko::Session::SetLocalDelay(Handle player, u8 delay)
{
    if (player - 1 < 0) {
        return;
    }

	for (u32 i = 0; i < _msg.locals.size(); i++) {
		if (_msg.locals[i]->handle == player) {
			_sync.SetLocalDelay(player, delay);
		}
	}
}

Gekko::Handle Gekko::Session::AddActor(PlayerType type, NetAddress* addr)
{
	if (type == Spectator) {
		if (_msg.spectators.size() >= _config.max_spectators) {
            return 0;
        }

		u32 new_handle = _config.num_players + (u32)_msg.spectators.size() + 1;
		_msg.spectators.push_back(std::make_unique<Player>(new_handle, type, addr));

		return new_handle;
	} 
	else {
        if (_started || _msg.locals.size() + _msg.remotes.size() >= _config.num_players) {
            return 0;
        }

		u32 new_handle = (u32)(_msg.locals.size() + _msg.remotes.size()) + 1;

		if (type == LocalPlayer) {
			_msg.locals.push_back(std::make_unique<Player>(new_handle, type, addr));
		} else {
			// require an address when specifing a remote player
            if (addr == nullptr) {
                return 0;
            }

			_msg.remotes.push_back(std::make_unique<Player>(new_handle, type, addr));
			_sync.SetInputPredictionWindow(new_handle, _config.input_prediction_window);
		}

		return new_handle;
	}
}

void Gekko::Session::AddLocalInput(Handle player, void* input)
{
	Input inp = (u8*)input;

	for (u32 i = 0; i < _msg.locals.size(); i++) {
		if (_msg.locals[i]->handle == player) {
			_sync.AddLocalInput(player, inp);
			break;
		}
	}
}

std::vector<Gekko::GameEvent*> Gekko::Session::UpdateSession()
{
	// connection Handling
	Poll();

	// clear GameEvents 
    _current_game_events.clear();

	// gameplay
	if (AllPlayersValid()) {
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
            return _current_game_events;
        }

		// then advance the session
		if (AddAdvanceEvent(_current_game_events)) {
			if (!_config.limited_saving ||
				(IsSpectating() || IsPlayingLocally()) &&
				_sync.GetCurrentFrame() % _config.input_prediction_window == 0) {
				AddSaveEvent(_current_game_events);
			}
			_sync.IncrementFrame();
		}
	}
	return _current_game_events;
}

std::vector<Gekko::SessionEvent*> Gekko::Session::Events()
{
    return _msg.session_events.GetRecentEvents();
}

Gekko::f32 Gekko::Session::FramesAhead()
{
    if (!_started) {
        return 0.f;
    }

	return _msg.history.GetAverageAdvantage();
}

void Gekko::Session::HandleSavingConfirmedFrame(std::vector<GameEvent*>& ev)
{
	if (!_config.limited_saving || IsSpectating() || IsPlayingLocally()) {
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
		AddAdvanceEvent(ev);
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

    const u8 delay = std::min(_config.spectator_delay, _config.MAX_SPECTATOR_DELAY);
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

void Gekko::Session::SendHealthCheck()
{
    if (!_config.desync_detection || IsSpectating()) {
        return;
    }

    const Frame min = _sync.GetMinReceivedFrame();
    const Frame sync = min - 1;

    if (_last_saved_frame < sync ||
        _last_sent_healthcheck >= sync) {
        return;
    }

    auto sav = _storage.GetState(sync);

    assert(sync == sav->frame);

    _last_sent_healthcheck = sync;
    _msg.SendHealthCheck(sync, sav->checksum);

    _msg.local_health[sync % Player::NUM_CHECKSUMS].frame = sync;
    _msg.local_health[sync % Player::NUM_CHECKSUMS].checksum = sav->checksum;
}

void Gekko::Session::AddDisconnectedPlayerInputs()
{
	for (auto& player : _msg.remotes) {
		if (player->GetStatus() == Disconnected) {
			_sync.AddRemoteInput(player->handle, _disconnected_input.get(), _sync.GetCurrentFrame());
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

void Gekko::Session::HandleRollback(std::vector<GameEvent*>& ev)
{
	Frame current = _sync.GetCurrentFrame();
	if (_last_saved_frame == GameInput::NULL_FRAME - 1) {
		_sync.SetCurrentFrame(current - 1);
		AddSaveEvent(ev);
		_sync.IncrementFrame();
	}

	if (_config.input_prediction_window == 0 || IsSpectating() || IsPlayingLocally()) {
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
		AddAdvanceEvent(ev);
		if (!_config.limited_saving || frame == frame_to_save) {
			AddSaveEvent(ev);
		}
		_sync.IncrementFrame();
	}

	// make sure that we are back where we started.
	assert(_sync.GetCurrentFrame() == current);
}

bool Gekko::Session::AddAdvanceEvent(std::vector<GameEvent*>& ev)
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

    if (event->data.adv.inputs) {
        std::memcpy(event->data.adv.inputs, inputs.get(), event->data.adv.input_len);
    }

	return true;
}

void Gekko::Session::AddSaveEvent(std::vector<GameEvent*>& ev)
{
	const Frame frame_to_save = _sync.GetCurrentFrame();

	auto state = _storage.GetState(frame_to_save);

	state->frame = frame_to_save;

    ev.push_back(_game_event_buffer.GetEvent(false));

	auto event = ev.back();
	event->type = SaveEvent;

	event->data.save.frame = frame_to_save;
	event->data.save.state = state->state;
	event->data.save.checksum = &state->checksum;
	event->data.save.state_len = &state->state_len;

	_last_saved_frame = frame_to_save;
}

void Gekko::Session::AddLoadEvent(std::vector<GameEvent*>& ev)
{
	const Frame frame_to_load = _sync.GetCurrentFrame();

	auto state = _storage.GetState(frame_to_load);

    ev.push_back(_game_event_buffer.GetEvent(false));

	auto event = ev.back();
	event->type = LoadEvent;

    event->data.load.frame = frame_to_load;
	event->data.load.state = state->state;
	event->data.load.state_len = &state->state_len;
}

void Gekko::Session::Poll()
{
    // reset session events
    _msg.session_events.Reset();

	// return if no host is defined.
    if (!_host) {
        return;
    }

    // fetch data from network
	auto data = _host->ReceiveData();

    // process the data we received
    _msg.HandleData(data);

	// handle received inputs
	HandleReceivedInputs();

	// update local frame advantage
	UpdateLocalFrameAdvantage();

	// add local input for the network
	SendLocalInputs();

	// send inputs to spectators 
	SendSpectatorInputs();

    // send a healthcheck if applicable
    SendHealthCheck();
    
	// now send data
	_msg.SendPendingOutput(_host);
}

bool Gekko::Session::AllPlayersValid()
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

	if (_config.post_sync_joining) {
		// TODO ACTUALLY HANDLE POST SYNC JOINING
		_msg.CheckStatusActors();
	}

	return true;
}

void Gekko::Session::HandleReceivedInputs()
{
	std::unique_ptr<NetInputData> current = nullptr;
	auto& received_inputs = _msg.LastReceivedInputs();
	while (!received_inputs.empty()) {
        // fetch input to be processed
		current = std::move(received_inputs.front());

        // remove input from the queue
		received_inputs.pop();

		// handle it as a spectator input if there are no local players.
        const bool spectating = IsSpectating();
		const u32 count = current->input.input_count;
		const u32 handles = spectating ? _config.num_players : (u32)current->handles.size();
		const u32 player_offset = current->input.total_size / handles;
		const Frame start = current->input.start_frame;

        for (u32 i = 0; i < handles; i++) {
            Handle handle = spectating ? i + 1 : current->handles[i];
            for (u32 j = 1; j <= count; j++) {
                Frame frame = start + j;
                u8* input = &current->input.inputs[(player_offset * i) + ((j - 1) * _config.input_size)];
                _sync.AddRemoteInput(handle, input, frame);
                if (j == count) {
                    _msg.SendInputAck(spectating ? current->handles[0] : handle, frame);
                }
            }
        }
	}
}

void Gekko::Session::SendLocalInputs()
{
	if (!_msg.locals.empty() && _started) {
		std::vector<Handle> handles;
		for (u32 i = 0; i < _msg.locals.size(); i++) {
			handles.push_back(_msg.locals[i]->handle);
		}

		const u8 delay = GetMinLocalDelay();
		const Frame current = _msg.GetLastAddedInput(false) + 1;

		std::unique_ptr<u8[]> inputs;
		for (Frame frame = current; frame <= current + delay; frame++) {
			if (!_sync.GetLocalInputs(handles, inputs, frame)) {
				break;
			}
			_msg.AddInput(frame, inputs.get());
		}
	}
}

Gekko::u8 Gekko::Session::GetMinLocalDelay()
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
