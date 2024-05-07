#include "input.h"

#include <cstdlib> 
#include <cstring>
#include <iostream>

void Gekko::InputBuffer::Init(u8 delay, u32 input_size)
{
	_input_delay = delay;
	_input_size = input_size;
	_last_received_input = GameInput::NULL_FRAME;

	// init GameInput array
	Input dummy = (u8*)std::malloc(_input_size);

	if(dummy)
		std::memset(dummy, 0, _input_size);

	for (u32 i = 0; i < BUFF_SIZE; i++) {
		_inputs.push_back(GameInput());
		_inputs[i].Init(GameInput::NULL_FRAME, dummy, _input_size);
	}

	std::free(dummy);
}

void Gekko::InputBuffer::AddLocalInput(Frame frame, const Input input)
{
	if (_inputs[frame % BUFF_SIZE].frame == GameInput::NULL_FRAME && _input_delay > 0) {
		for (i32 i = 0; i < _input_delay; i++) {
			Input dummy = (u8*)std::malloc(_input_size);

			if (dummy)
				std::memset(dummy, 0, _input_size);

			AddInput(i,  dummy);

			std::free(dummy);
		}
	}
	AddInput(frame + _input_delay, input);
}

void Gekko::InputBuffer::AddInput(Frame frame, Input input)
{
	if (frame == _last_received_input + 1) {
		_last_received_input++;
		_inputs[frame % BUFF_SIZE].Init(frame, input, _input_size);
	}
}

void Gekko::InputBuffer::SetDelay(u8 delay)
{
	// early return AddLocalInput will handle it
	if (_last_received_input == GameInput::NULL_FRAME || _input_delay == delay) {
		_input_delay = delay;
		return;
	}

	// when out current delay is smaller then the new delay 
	// all we have to do is expand the delay with the last input we received
	if (_input_delay < delay)
	{
		_input_delay = delay;
		Frame last_input = _last_received_input;

		for (i32 i = 1; i <= _input_delay; i++) {
			Input dummy = (u8*)std::malloc(_input_size);

			if (dummy)
				std::memcpy(dummy,_inputs[last_input % BUFF_SIZE].input, _input_size);

			AddInput(last_input + i, dummy);

			std::free(dummy);
		}
		return;
	}

	// if our current delay is bigger then our new delay the plan is different. 
	// since we probably already sent the inputs up to _last_received_input 
	// which includes the previous delay we should not modify any inputs that happend before that
	// this will probably mean that it will drop local inputs until the local machine catches up
	if (_input_delay > delay) {
		_input_delay = delay;
		return;
	}
}

Gekko::u8 Gekko::InputBuffer::GetDelay() 
{
	return _input_delay;
}

std::unique_ptr<Gekko::GameInput> Gekko::InputBuffer::GetInput(Frame frame)
{
	std::unique_ptr<GameInput> inp(new GameInput());
	
	inp->frame = GameInput::NULL_FRAME;
	inp->input = nullptr;
	inp->input_len = 0;

	if(_last_received_input < frame)
		return inp;

	if (_inputs[frame % BUFF_SIZE].frame == GameInput::NULL_FRAME)
		return inp;

	if (_inputs[frame % BUFF_SIZE].frame != frame)
		return inp;

	inp->Init(_inputs[frame % BUFF_SIZE]);
	return inp;
}

void Gekko::GameInput::Init(GameInput& other)
{
	frame = other.frame;
	input_len = other.input_len;

	if (input) {
		std::memcpy(input, other.input, input_len);
		return;
	}

	input = (Input) std::malloc(input_len);

	if (input)
		std::memcpy(input, other.input, input_len);
}

void Gekko::GameInput::Init(Frame frame_num, Input inp, u32 inp_len)
{
	frame = frame_num;
	input_len = inp_len;

	if (input) {
		std::memcpy(input, inp, input_len);
		return;
	}

	input = (Input) std::malloc(input_len);

	if (input)
		std::memcpy(input, inp, input_len);
}

Gekko::GameInput::GameInput()
{
	frame = NULL_FRAME;
	input_len = 0;
	input = nullptr;
}

Gekko::GameInput::~GameInput()
{
	if (input) {
		std::free(input);
		input = nullptr;
	}
}
