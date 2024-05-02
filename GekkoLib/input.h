#pragma once

#include "gekko_types.h"
#include <deque>
#include <memory>

namespace Gekko {
	struct GameInput {
		static const i32 NULL_FRAME = -1;

		void Init(GameInput& other);
		void Init(Frame frame_num, Input inp, u32 inp_len);

		GameInput();
		~GameInput();

		Frame frame;
		Input input;
		u32 input_len;
	};

	struct InputBuffer {
		static const u32 BUFF_SIZE = 128;

		void Init(u8 delay, u32 input_size);

		InputBuffer() = default;

		void AddLocalInput(Frame frame, Input input);
		void AddInput(Frame frame, Input input);

		void SetDelay(u8 delay);

		std::unique_ptr<GameInput> GetInput(Frame frame);

	private:
		u8 _input_delay;
		u32 _input_size;
		Frame _last_received_input;

		std::deque<GameInput> _inputs;
	};
}