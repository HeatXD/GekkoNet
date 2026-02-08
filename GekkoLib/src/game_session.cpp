#include "session.h"

#include <cassert>

Gekko::GameSession::GameSession()
{
	_host = nullptr;
	_started = false;
    _last_saved_frame = GameInput::NULL_FRAME - 1;
	_disconnected_input = nullptr;
    _last_sent_healthcheck = GameInput::NULL_FRAME;
    _config = GekkoConfig();
}

void Gekko::GameSession::Init(GekkoConfig* config)
{
    _host = nullptr;

    _started = false;

    // get given configs
    std::memcpy(&_config, config, sizeof(GekkoConfig));

    // setup input buffer for the players
    _sync.Init(_config.num_players, _config.input_size);

    // setup message system.
    _msg.Init(_config.num_players, _config.input_size);

    // setup game event system
    _game_events.Init(_config.input_size * _config.num_players);

    // setup state storage
    _storage.Init(_config.input_prediction_window, _config.state_size, _config.limited_saving);

    // setup disconnected input for disconnected player within the session
    _disconnected_input = std::make_unique<u8[]>(_config.input_size);
    std::memset(_disconnected_input.get(), 0, _config.input_size);

    // we only detect desyncs whenever we are not limited saving for now.
    _config.desync_detection = _config.limited_saving ? false : _config.desync_detection;
}

void Gekko::GameSession::SetLocalDelay(i32 player, u8 delay)
{
    for (u32 i = 0; i < _msg.locals.size(); i++) {
        if (_msg.locals[i]->handle == player) {
            _sync.SetLocalDelay(player, delay);
        }
    }
}

void Gekko::GameSession::SetNetAdapter(GekkoNetAdapter* adapter)
{
    _host = adapter;
}

i32 Gekko::GameSession::AddActor(GekkoPlayerType type, GekkoNetAddress* addr)
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

void Gekko::GameSession::AddLocalInput(i32 player, void* input)
{
    u8* inp = (u8*)input;

    for (u32 i = 0; i < _msg.locals.size(); i++) {
        if (_msg.locals[i]->handle == player) {
            _sync.AddLocalInput(player, inp);
            break;
        }
    }
}

GekkoGameEvent** Gekko::GameSession::UpdateSession(i32* count)
{
    // reset session events
    _msg.session_events.Reset();

    // connection Handling
    Poll();

    // clear GameEvents
    _game_events.Clear();

    // gameplay
    if (AllActorsValid()) {
        // reset the game event buffer before doing anything else
        _game_events.Reset();

        // add inputs so we can continue the session.
        AddDisconnectedPlayerInputs();

        // check if we need to rollback
        HandleRollback();

        // check if we need to save the confirmed frame
        HandleSavingConfirmedFrame();

        // send a healthcheck if applicable
        SendSessionHealthCheck();

        // check if the session is still doing alright.
        SessionIntegrityCheck();

        // then advance the session
        if (_game_events.AddAdvanceEvent(_sync, false)) {
            if (!_config.limited_saving ||
                IsPlayingLocally() &&
                _sync.GetCurrentFrame() % _config.input_prediction_window == 0) {
                _game_events.AddSaveEvent(_sync, _storage, &_last_saved_frame);
            }
            _sync.IncrementFrame();
        }
    }

    *count = _game_events.Count();
    return _game_events.Data();
}

GekkoSessionEvent** Gekko::GameSession::Events(i32* count)
{
    *count = (i32)_msg.session_events.GetRecentEvents().size();
    return _msg.session_events.GetRecentEvents().data();
}

f32 Gekko::GameSession::FramesAhead()
{
    if (!_started) {
        return 0.f;
    }

	return _msg.history.GetAverageAdvantage();
}

void Gekko::GameSession::NetworkStats(i32 player, GekkoNetworkStats* stats)
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

void Gekko::GameSession::NetworkPoll()
{
    Poll();
}

void Gekko::GameSession::HandleSavingConfirmedFrame()
{
	if (IsLockstepActive() || !_config.limited_saving ||
        IsPlayingLocally()) {
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
	_game_events.AddLoadEvent(_sync, _storage);
	_sync.IncrementFrame();

	for (Frame frame = sync_frame + 1; frame < current; frame++) {
        _game_events.AddAdvanceEvent(_sync, true);
		if (frame == frame_to_save) {
            _game_events.AddSaveEvent(_sync, _storage, &_last_saved_frame);
		}
		_sync.IncrementFrame();
	}

	// make sure that we are back where we started.
	assert(_sync.GetCurrentFrame() == current);
}

void Gekko::GameSession::UpdateLocalFrameAdvantage()
{
    if (!_started) {
        return;
    }

	const Frame min = _sync.GetMinReceivedFrame();
	const Frame current = _sync.GetCurrentFrame();
    const i32 local_advantage = current - min;

	_msg.history.SetLocalAdvantage(local_advantage);
}

void Gekko::GameSession::SendSessionHealthCheck()
{
    if (!_config.desync_detection) {
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

void Gekko::GameSession::SendNetworkHealthCheck()
{
    _msg.SendNetworkHealth();
}

void Gekko::GameSession::SessionIntegrityCheck()
{
    if (!_config.desync_detection) {
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

void Gekko::GameSession::AddDisconnectedPlayerInputs()
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

void Gekko::GameSession::SendSpectatorInputs()
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

void Gekko::GameSession::HandleRollback()
{
	Frame current = _sync.GetCurrentFrame();
	if (_last_saved_frame == GameInput::NULL_FRAME - 1) {
		_sync.SetCurrentFrame(current - 1);
		_game_events.AddSaveEvent(_sync, _storage, &_last_saved_frame);
		_sync.IncrementFrame();
	}

	if (IsLockstepActive() || IsPlayingLocally()) {
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
	_game_events.AddLoadEvent(_sync, _storage);
	_sync.IncrementFrame();

	for (Frame frame = sync_frame + 1; frame < current; frame++) {
        _game_events.AddAdvanceEvent(_sync, true);
		if (!_config.limited_saving || frame == frame_to_save) {
            _game_events.AddSaveEvent(_sync, _storage, &_last_saved_frame);
		}
		_sync.IncrementFrame();
	}

    // clear the marked mispredictions up to this point in the input buffer
    _sync.ClearIncorrectFramesUpTo(current);

	// make sure that we are back where we started.
	assert(_sync.GetCurrentFrame() == current);
}

void Gekko::GameSession::Poll()
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

bool Gekko::GameSession::AllActorsValid()
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

void Gekko::GameSession::HandleReceivedInputs()
{
    for (auto& remote : _msg.remotes) {
        if (remote->GetStatus() != Connected) continue;

        {
            auto handle = remote->handle;
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

void Gekko::GameSession::SendLocalInputs()
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

u8 Gekko::GameSession::GetMinLocalDelay()
{
	u8 min = UINT8_MAX;
	for (auto& player : _msg.locals) {
		min = std::min(_sync.GetLocalDelay(player->handle), min);
	}
	return min;
}

bool Gekko::GameSession::IsPlayingLocally()
{
	return _msg.remotes.empty() && !_msg.locals.empty();
}

bool Gekko::GameSession::IsLockstepActive() const
{
    return _config.input_prediction_window == 0;
}
