#pragma once

#include <vector>

#include "gekko_types.h"
#include "input.h"

namespace Gekko {
	class SyncSystem {
	public:
		SyncSystem();
		~SyncSystem();

		void Init(u8 num_players, u32 input_size, u8 input_delay);

		void AddLocalInput(Handle player, Input input);

		void IncrementFrame();

		bool GetCurrentInputs(std::unique_ptr<u8[]>& inputs, Frame& frame);

	private:
		u8 _num_players;
		u32 _input_size;
		Frame _current_frame;
		InputBuffer* _input_buffers;
	};
}