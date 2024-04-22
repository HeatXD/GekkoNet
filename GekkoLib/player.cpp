#include "player.h"

Gekko::Player::Player(PlayerType type, NetAddress address)
{
	_type = type;
	_address = address;

	_stats = NetStats();
	_status = _type == Local ? Connected : Initiating;
}
