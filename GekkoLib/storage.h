#pragma once

#include "gekko_types.h"
#include "input.h"

#include <memory>
#include <vector>

namespace Gekko {
	struct StateEntry {
		Frame frame = GameInput::NULL_FRAME;
		u8* state = nullptr;
		u32 state_len = 0;
		u32 checksum = 0;
	};

	class StateStorage {
	public:
		StateStorage();

		void Init(u32 num_states);

		StateEntry* GetState(Frame frame);

	private:
		u32 _max_num_states;

		std::vector<std::unique_ptr<StateEntry>> _states;
	};
}