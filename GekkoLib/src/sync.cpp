#include "sync.h"

#include <memory>
#include <cstring>
#include <climits>

Gekko::SyncSystem::SyncSystem()
{
	_current_frame = GameInput::NULL_FRAME;
	_input_size = 0;
	_num_players = 0;
	_input_buffers = nullptr;
}

void Gekko::SyncSystem::Init(u8 num_players, u32 input_size)
{
	_input_size = input_size;
	_num_players = num_players;
	_current_frame = GameInput::NULL_FRAME + 1;

    _input_buffers = std::make_unique<InputBuffer[]>(num_players);
	// on creation setup input buffers
	for (int i = 0; i < _num_players; i++) {
		_input_buffers[i].Init(0, 0, input_size);
	}
}

void Gekko::SyncSystem::AddLocalInput(Handle player, u8* input)
{
	// drop inputs from incorrect handles
    if (player >= _num_players || player < 0) {
        return;
    }

	_input_buffers[player].AddLocalInput(_current_frame, input);
}

void Gekko::SyncSystem::AddRemoteInput(Handle player, u8* input, Frame frame)
{
	// drop inputs from incorrect handles
    if (player >= _num_players || player < 0) {
        return;
    }

	_input_buffers[player].AddInput(frame, input);
}

void Gekko::SyncSystem::IncrementFrame()
{
	_current_frame++;
}

bool Gekko::SyncSystem::GetSpectatorInputs(std::unique_ptr<u8[]>& inputs, Frame frame) 
{
    auto all_input = std::make_unique<u8[]>(_input_size * _num_players);
	for (u8 i = 0; i < _num_players; i++) {
		auto inp = _input_buffers[i].GetInput(frame);

		if (inp->frame == GameInput::NULL_FRAME) {
			return false;
		}

		std::memcpy(all_input.get() + (i * _input_size), (void*)inp->input.get(), _input_size);
	}
	inputs = std::move(all_input);
	return true;
}

bool Gekko::SyncSystem::GetCurrentInputs(std::unique_ptr<u8[]>& inputs, Frame& frame)
{
    auto all_input = std::make_unique<u8[]>(_input_size * _num_players);
	for (u8 i = 0; i < _num_players; i++) {
		auto inp = _input_buffers[i].GetInput(_current_frame, true);
	
		if (inp->frame == GameInput::NULL_FRAME) {
			return false;
		}

		std::memcpy(all_input.get() + (i * _input_size), (void*)inp->input.get(), _input_size);
	}
	frame = _current_frame;
	inputs = std::move(all_input);
	return true;
}

bool Gekko::SyncSystem::GetLocalInput(Handle player, std::unique_ptr<u8[]>& input, Frame frame)
{	
	auto inp = _input_buffers[player].GetInput(frame);
	if (inp->frame == GameInput::NULL_FRAME) {	
		return false;
	}
	std::memcpy(input.get(), inp->input.get(), _input_size);
	return true;
}

void Gekko::SyncSystem::SetLocalDelay(Handle player, u8 delay)
{
	_input_buffers[player].SetDelay(delay);
}

u8 Gekko::SyncSystem::GetLocalDelay(Handle player)
{
	return _input_buffers[player].GetDelay();
}

void Gekko::SyncSystem::SetInputPredictionWindow(Handle player, u8 input_window)
{
	_input_buffers[player].SetInputPredictionWindow(input_window);
}

Frame Gekko::SyncSystem::GetCurrentFrame()
{
	return _current_frame;
}

void Gekko::SyncSystem::SetCurrentFrame(Frame frame)
{
	_current_frame = frame;
}

Frame Gekko::SyncSystem::GetMinIncorrectFrame()
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

Frame Gekko::SyncSystem::GetMinReceivedFrame()
{
	Frame min = INT_MAX;
	for (i32 i = 0; i < _num_players; i++) {
		Frame frame = _input_buffers[i].GetLastReceivedFrame();
		min = std::min(frame, min);
	}
	return min == INT_MAX ? GameInput::NULL_FRAME : min;
}

Frame Gekko::SyncSystem::GetLastReceivedFrom(Handle player)
{
    if (player >= 0 && player < _num_players) {
        return _input_buffers[player].GetLastReceivedFrame();
    }

    return GameInput::NULL_FRAME;
}

void Gekko::SyncSystem::ClearIncorrectFramesUpTo(Frame clear_limit)
{
    for (i32 i = 0; i < _num_players; i++) {
        _input_buffers[i].ClearIncorrectFrames(clear_limit);
    }
}
