#include "storage.h"

Gekko::StateStorage::StateStorage()
{
	_max_num_states = 0;
}


void Gekko::StateStorage::Init(u32 num_states, u32 state_size, bool limited)
{
	const u32 num = limited ? 1 : num_states;
	_max_num_states = num;

	for (u32 i = 0; i < _max_num_states; i++) {
		_states.push_back(std::unique_ptr<StateEntry>(new StateEntry));
		_states.back().get()->state = (u8*)std::malloc(state_size);
		_states.back().get()->state_len = state_size;
	}
}

Gekko::StateEntry* Gekko::StateStorage::GetState(Frame frame)
{
	frame = frame < 0 ? frame + _max_num_states : frame;
	return _states[frame % _max_num_states].get();
}

Gekko::StateEntry::~StateEntry()
{
	if (state) {
		std::free(state);
	}
}
