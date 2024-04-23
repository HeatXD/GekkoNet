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

	struct Advance
	{
		Frame frame;
		u32 input_len;
		u8* inputs;
	};

	struct Save
	{
		Frame frame;
		u64 checksum;
		u32 state_len;
		u8* state;
	};

	struct Load
	{
		Frame frame;
		u32 state_len;
		u8* state;
	};

	struct EventData {
		EventData();
		union data{
			data();

			// events 
			Advance adv;
			Save save;
			Load load;
			
			u32 x;
		} ev;
	};

	struct Event {
		Event();
		EventType type;
		EventData data;
	};
}