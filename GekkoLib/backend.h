#pragma once

#include "gekko_types.h"
#include <memory>

namespace Gekko {
	class NetAddress {
	public:
		NetAddress();
		NetAddress(u8* data, u32 size);

		u8* GetAddress();
		u32 GetSize();

		void Copy(NetAddress& other);

	private:
		std::unique_ptr <u8[]> _data;
		u32 _size;
	};

	struct NetPacket {
	};

	struct NetStats {
	};
}