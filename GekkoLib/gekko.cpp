#include "gekko.h"
#include "backend.h"

#include <iostream>
#include <cassert>

void Gekko::Session::Test()
{
	std::cout << "Hello Gekko!\n";
}

Gekko::Session::Session()
{
	_host = nullptr;
	_started = false;
	_last_saved_frame = GameInput::NULL_FRAME - 1;
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
	_config = config;

	// setup input buffer for the players
	_sync.Init(_config.num_players, _config.input_size);

	// setup message system.
	_msg.Init(_config.input_size);

	// setup state storage
	_storage.Init(_config.input_prediction_window, _config.state_size, _config.limited_saving);
}

void Gekko::Session::SetLocalDelay(Handle player, u8 delay)
{
	if (player - 1 >= 0) {
		for (u32 i = 0; i < _msg.locals.size(); i++) {
			if (_msg.locals[i]->handle == player) {
				_sync.SetLocalDelay(player, delay);
			}
		}
	}
}

Gekko::Handle Gekko::Session::AddActor(PlayerType type, NetAddress* addr)
{
	if (type == Spectator) {
		if (_msg.spectators.size() >= _config.max_spectators)
			return 0;

		u32 new_handle = _config.num_players + (u32)_msg.spectators.size() + 1;
		_msg.spectators.push_back(new Player(new_handle, type, addr));

		return new_handle;
	} 
	else {
		if (_started || _msg.locals.size() + _msg.remotes.size() >= _config.num_players)
			return 0;

		u32 new_handle = (u32)(_msg.locals.size() + _msg.remotes.size()) + 1;

		if (type == LocalPlayer) {
			_msg.locals.push_back(new Player(new_handle, type, addr));
		} else {
			// require an address when specifing a remote player
			if (addr == nullptr)
				return 0;

			_msg.remotes.push_back(new Player(new_handle, type, addr));
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

std::vector<Gekko::Event> Gekko::Session::UpdateSession()
{
	// connection Handling
	Poll();

	// setup events 
	auto ev = std::vector<Event>();

	// gameplay
	if (AllPlayersValid()) {
		// add inputs so we can continue the session.
		AddDisconnectedPlayerInputs();

		// check if we need to rollback
		HandleRollback(ev);

		// check if we need to save the confirmed frame
		HandleSavingConfirmedFrame(ev);

		// then advance the session
		if (AddAdvanceEvent(ev)) {
			if (!_config.limited_saving ||
				(IsSpectating() || IsPlayingLocally()) &&
				_sync.GetCurrentFrame() % _config.input_prediction_window == 0) {
				AddSaveEvent(ev);
			}
			_sync.IncrementFrame();
		}
	}
	return ev;
}

Gekko::f32 Gekko::Session::FramesAhead()
{
	if (!_started)
		return 0;

	return _msg.history.GetAverageAdvantage();
}

void Gekko::Session::HandleSavingConfirmedFrame(std::vector<Event>& ev)
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
	if (!_started)
		return;

	const Frame min = _sync.GetMinReceivedFrame();
	const Frame current = _sync.GetCurrentFrame();
	const i32 local_advantage = current - min;

	_msg.history.SetLocalAdvantage(local_advantage);
}

void Gekko::Session::AddDisconnectedPlayerInputs()
{
	for (auto player : _msg.remotes) {
		if (player->GetStatus() == Disconnected) {
			_sync.AddRemoteInput(player->handle, 0, _sync.GetCurrentFrame());
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

void Gekko::Session::HandleRollback(std::vector<Event>& ev)
{
	Frame current = _sync.GetCurrentFrame();
	if (_last_saved_frame == GameInput::NULL_FRAME - 1) {
		_sync.SetCurrentFrame(current - 1);
		AddSaveEvent(ev);
		_sync.IncrementFrame();
	}

	if (_config.input_prediction_window == 0 || IsSpectating() || IsPlayingLocally())
		return;

	current = _sync.GetCurrentFrame();
	const Frame min = _sync.GetMinIncorrectFrame();

	// dont allow rollbacks starting before the null frame
	if (min == GameInput::NULL_FRAME)
		return;

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

bool Gekko::Session::AddAdvanceEvent(std::vector<Event>& ev)
{
	Frame frame = GameInput::NULL_FRAME;
	std::unique_ptr<u8[]> inputs;
	if (!_sync.GetCurrentInputs(inputs, frame))
		return false;

	Event event;
	event.type = AdvanceEvent;
	event.data.ev.adv.frame = frame;
	event.data.ev.adv.input_len = _config.num_players * _config.input_size;
	event.data.ev.adv.inputs = (u8*)std::malloc(event.data.ev.adv.input_len);

	if (event.data.ev.adv.inputs)
		std::memcpy(event.data.ev.adv.inputs, inputs.get(), event.data.ev.adv.input_len);

	ev.push_back(event);

	return true;
}

void Gekko::Session::AddSaveEvent(std::vector<Event>& ev)
{
	const Frame frame_to_save = _sync.GetCurrentFrame();

	auto state = _storage.GetState(frame_to_save);

	state->frame = frame_to_save;

	Event event;
	event.type = SaveEvent;
	event.data.ev.save.frame = frame_to_save;

	event.data.ev.save.state = state->state;
	event.data.ev.save.checksum = &state->checksum;
	event.data.ev.save.state_len = &state->state_len;

	ev.push_back(event);

	_last_saved_frame = frame_to_save;
}

void Gekko::Session::AddLoadEvent(std::vector<Event>& ev)
{
	const Frame frame_to_load = _sync.GetCurrentFrame();

	auto state = _storage.GetState(frame_to_load);

	Event event;
	event.type = LoadEvent;
	event.data.ev.load.frame = frame_to_load;

	event.data.ev.load.state = state->state;
	event.data.ev.load.state_len = &state->state_len;

	ev.push_back(event);
}

void Gekko::Session::Poll()
{
	// return if no host is defined.
	if (!_host)
		return;

	auto data = _host->ReceiveData();
	_msg.HandleData(data, _started);

	// handle received inputs
	HandleReceivedInputs();

	// update local frame advantage
	UpdateLocalFrameAdvantage();

	// add local input for the network
	SendLocalInputs();

	// send inputs to spectators 
	SendSpectatorInputs();

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
		_started = true;
		printf("__ session started __\n");
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
	NetInputData* current = nullptr;
	auto& received_inputs = _msg.LastReceivedInputs();
	while (!received_inputs.empty()) {
		current = received_inputs.front();
		received_inputs.pop();

		// handle it as a spectator input if there are no local players.
		if (IsSpectating()) {
			const u32 count = current->input.input_count;
			const u32 inp_len_per_frame = current->input.total_size / count;
			const Frame start = current->input.start_frame;
			const Handle handle = _msg.remotes[0]->handle;

			for (u32 i = 1; i <= count; i++) {
				for (u32 j = 0; j < _config.num_players; j++) {
					Frame frame = start + i;
					u8* input = &current->input.inputs[((i - 1) * inp_len_per_frame) + (j * _config.input_size)];
					_sync.AddRemoteInput(j + 1, input, frame);
					if (i == count) {
						_msg.SendInputAck(handle, frame);
					}
				}
			}
		}
		else {
			const u32 count = current->input.input_count;
			const u32 handles = (u32)current->handles.size();
			const u32 inp_len_per_frame = current->input.total_size / count;
			const Frame start = current->input.start_frame;
	
			for (u32 i = 1; i <= count; i++) {
				for (u32 j = 0; j < handles; j++) {
					Frame frame = start + i;
					Handle handle = current->handles[j];
					u8* input = &current->input.inputs[((i - 1) * inp_len_per_frame) + (j * _config.input_size)];
					_sync.AddRemoteInput(handle, input, frame);
					if (i == count) {
						_msg.SendInputAck(handle, frame);
					}
				}
			}
		}

		// free the inputs since we used malloc.		
		std::free(current->input.inputs);
		delete current;
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
	for (auto player : _msg.locals) {
		min = std::min(_sync.GetLocalDelay(player->handle), min);
	}
	return min;
}

bool Gekko::Session::IsSpectating() {
	return _msg.remotes.size() == 1 && _msg.locals.empty();
}


bool Gekko::Session::IsPlayingLocally() {
	return _msg.remotes.empty() && !_msg.locals.empty();
}