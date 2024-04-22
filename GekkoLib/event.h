#pragma once

#include <vector>
#include <memory>

#include "gekko_types.h"

namespace Gekko {
	enum EventType {
		None = 0,
		AdvanceEvent
	};

	struct Advance
	{
		Advance() = default;
		~Advance();

		Frame frame;
		u32 input_len;
		u8* inputs;
	};

	struct EventData {
		EventData();
		union x {
			~x();
			Advance adv;
			u32 xxx;
		}ev;
	};

	struct Event {
		Event();
		EventType type;
		EventData data;
	};
}