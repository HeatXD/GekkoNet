#include "sync.h"
#include <memory>

Gekko::SyncSystem::SyncSystem()
{
	_current_frame = GameInput::NULL_FRAME;
	_input_size = 0;
	_num_players = 0;
	_input_buffers = nullptr;
}

Gekko::SyncSystem::~SyncSystem()
{
	delete _input_buffers;
}

void Gekko::SyncSystem::Init(u8 num_players, u32 input_size)
{
	_input_size = input_size;
	_num_players = num_players;
	_current_frame = GameInput::NULL_FRAME + 1;

	_input_buffers = new InputBuffer[_num_players];

	// on creation setup input buffers
	for (int i = 0; i < _num_players; i++) {
		_input_buffers[i].Init(0, 0, input_size);
	}
}

void Gekko::SyncSystem::AddLocalInput(Handle player, Input input)
{
	// valid handles start from 1 while the input buffers index starts at 0.
	i32 plyr = player - 1;

	// drop inputs from incorrect handles
    if (plyr >= _num_players || plyr < 0) {
        return;
    }

	_input_buffers[plyr].AddLocalInput(_current_frame, input);
}

void Gekko::SyncSystem::AddRemoteInput(Handle player, Input input, Frame frame)
{
	// valid handles start from 1 while the input buffers index starts at 0.
	i32 plyr = player - 1;

	// drop inputs from incorrect handles
    if (plyr >= _num_players || plyr < 0) {
        return;
    }

	_input_buffers[plyr].AddInput(frame, input);
}

void Gekko::SyncSystem::IncrementFrame()
{
	_current_frame++;
}

bool Gekko::SyncSystem::GetSpectatorInputs(std::unique_ptr<u8[]>& inputs, Frame frame) 
{
	std::unique_ptr<u8[]> all_input(new u8[_input_size * _num_players]);
	for (u8 i = 0; i < _num_players; i++) {
		auto inp = _input_buffers[i].GetInput(frame, false);

		if (inp->frame == GameInput::NULL_FRAME) {
			return false;
		}

		std::memcpy(all_input.get() + (i * _input_size), inp.get()->input, _input_size);
	}
	inputs = std::move(all_input);
	return true;
}

bool Gekko::SyncSystem::GetCurrentInputs(std::unique_ptr<u8[]>& inputs, Frame& frame)
{
	std::unique_ptr<u8[]> all_input(new u8[_input_size * _num_players]);
	for (u8 i = 0; i < _num_players; i++) {
		auto inp = _input_buffers[i].GetInput(_current_frame, true);
	
		if (inp->frame == GameInput::NULL_FRAME) {
			return false;
		}

		std::memcpy(all_input.get() + (i * _input_size), inp.get()->input, _input_size);
	}
	frame = _current_frame;
	inputs = std::move(all_input);
	return true;
}

bool Gekko::SyncSystem::GetLocalInputs(std::vector<Handle>& handles, std::unique_ptr<u8[]>& inputs, Frame frame)
{	
	inputs.reset();
	std::unique_ptr<u8[]> all_input(new u8[_input_size * handles.size()]);
	for (u8 i = 0; i < handles.size(); i++) {
		auto inp = _input_buffers[handles[i] - 1].GetInput(frame, true);

		if (inp->frame == GameInput::NULL_FRAME) {	
			return false;
		}

		std::memcpy(all_input.get() + (i * _input_size), inp.get()->input, _input_size);
	}
	inputs = std::move(all_input);
	return true;
}

void Gekko::SyncSystem::SetLocalDelay(Handle player, u8 delay)
{
	_input_buffers[player - 1].SetDelay(delay);
}

Gekko::u8 Gekko::SyncSystem::GetLocalDelay(Handle player)
{
	return _input_buffers[player - 1].GetDelay();
}

void Gekko::SyncSystem::SetInputPredictionWindow(Handle player, u8 input_window)
{
	_input_buffers[player - 1].SetInputPredictionWindow(input_window);
}

Gekko::Frame Gekko::SyncSystem::GetCurrentFrame()
{
	return _current_frame;
}

void Gekko::SyncSystem::SetCurrentFrame(Frame frame)
{
	_current_frame = frame;
}

Gekko::Frame Gekko::SyncSystem::GetMinIncorrectFrame()
{
	Frame min = INT_MAX;
	for (i32 i = 0; i < _num_players; i++) {
		Frame frame = _input_buffers[i].GetIncorrectPredictionFrame();
		if (frame == GameInput::NULL_FRAME) {
			continue;
		}
		min = std::min(frame, min);
	}
	return min == INT_MAX ? GameInput::NULL_FRAME : min;
}

Gekko::Frame Gekko::SyncSystem::GetMinReceivedFrame()
{
	Frame min = INT_MAX;
	for (i32 i = 0; i < _num_players; i++) {
		Frame frame = _input_buffers[i].GetLastReceivedFrame();
		min = std::min(frame, min);
	}
	return min == INT_MAX ? GameInput::NULL_FRAME : min;
}
