#pragma once

#include <vector>
#include <memory>

#include "gekko_types.h"

namespace Gekko {
	enum EventType {
		None = 0,
		AdvanceEvent,
		SaveEvent,
		LoadEvent
	};

	struct EventData {
		union data {
			// events 
			struct Advance {
				Frame frame;
				u32 input_len;
				u8* inputs;
			} adv;
			struct Save {
				Frame frame;
				u32* checksum;
				u32* state_len;
				u8* state;
			} save;
			struct Load {
				Frame frame;
				u32* state_len;
				u8* state;
			} load;
			u32 x = 0;
		} ev;
	};

	struct Event {
		EventType type = None;
		EventData data;
	};
}
