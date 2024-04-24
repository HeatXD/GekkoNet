#pragma once

#include "gekko_types.h"
#include "backend.h"
#include <vector>
#include <utility>

namespace Gekko {
	class NetAdapter {
	public:
		virtual std::vector<std::pair<NetAddress, NetPacket>> ReceiveMessages() = 0;
		virtual void SendMessage(NetAddress addr, u8* pkt, u32 pkt_len) = 0;
	};
}