#pragma once

#include "net_adapter.h"

namespace Gekko {

	enum PlayerType {
		Local,
		Remote,
		Spectator
	};

	enum PlayerStatus {
		Initiating,
		Connected,
		Running,
		Disconnected,
	};

	class Player
	{
	public:
		Player(PlayerType type, NetAddress address);

		PlayerType GetType();

		PlayerStatus GetStatus();

	private:
		PlayerType _type;
		PlayerStatus _status;

		NetStats _stats;
		NetAddress _address;
	};
}