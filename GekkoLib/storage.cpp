#include "storage.h"

Gekko::StateStorage::StateStorage()
{
	_max_num_states = 0;
}

void Gekko::StateStorage::Init(u32 num_states)
{
	_max_num_states = num_states + 2;

	for (u32 i = 0; i < _max_num_states; i++) {
		_states.push_back(std::unique_ptr<StateEntry>(new StateEntry));
	}
}

Gekko::StateEntry* Gekko::StateStorage::GetState(Frame frame)
{
	frame = frame < 0 ? _max_num_states - frame : frame;
	return _states[frame % _max_num_states].get();
}

