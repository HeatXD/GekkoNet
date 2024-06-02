#pragma once

#include "gekko_types.h"

#include <vector>
#include <memory>

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

    struct SysEvent {
        enum Type {
            None = 0,
            Syncing,
            Connected,
            Disconnected
        } type = None;

        union Data {
            struct Syncing {
                Handle handle;
                u8 current;
                u8 max;
            } syncing;
            struct Connected {
                Handle handle;
            } connected;
            struct Disconnected {
                Handle handle;
            } disconnected;
        } data;
    };
}
