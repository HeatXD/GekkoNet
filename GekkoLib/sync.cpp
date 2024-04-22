#include "sync.h"
#include <memory>

Gekko::SyncSystem::SyncSystem()
{
	_current_frame = GameInput::NULL_FRAME;
	_input_size = 0;
	_num_players = 0;
}

void Gekko::SyncSystem::Init(u8 num_players, u32 input_size, u8 input_delay)
{
	_input_size = input_size;
	_num_players = num_players;
	_current_frame = 0;

	_input_buffers = new InputBuffer[_num_players];

	// on creation setup input buffers
	for (int i = 0; i < _num_players; i++) {
		_input_buffers[i].Init(input_delay, input_size);
	}
}

void Gekko::SyncSystem::AddLocalInput(Handle player, Input input)
{
	// valid handles start from 1 while the input buffers index starts at 0.
	i32 plyr = player - 1;

	// drop inputs from incorrect handles
	if (plyr >= _num_players|| plyr < 0)
		return;

	_input_buffers[plyr].AddLocalInput(_current_frame, input);
}

void Gekko::SyncSystem::IncrementFrame()
{
	_current_frame++;
}

bool Gekko::SyncSystem::GetCurrentInputs(std::unique_ptr<u8[]>& inputs, Frame& frame)
{
	std::unique_ptr<u8[]> all_input(new u8[_input_size * _num_players]);
	for (u8 i = 0; i < _num_players; i++) {
		auto inp = _input_buffers[i].GetInput(_current_frame);
	
		if (inp->frame == GameInput::NULL_FRAME)
			return false;

		std::memcpy(all_input.get() + (i * _input_size), inp.get()->input, _input_size);
	}
	frame = _current_frame;
	inputs = std::move(all_input);
	return true;
}

//bool Gekko::SyncSystem::GetCurrentInputs(std::vector<Input>& inputs, Frame& frame)
//{
//	for (u8 i = 0; i < _num_players; i++) {
//		auto inp = _input_buffers[i].GetInput(_current_frame);
//
//		if (inp->frame == GameInput::NULL_FRAME)
//			return false;
//
//		inputs.push_back(inp);
//	}
//	frame = _current_frame;
//	return true;
//}
