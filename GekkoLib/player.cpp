#include "player.h"

Gekko::Player::Player(PlayerType type, NetAddress& address)
{
	_type = type;
	_address.Copy(address);

	_stats = NetStats();
	_status = _type == Local ? Connected : Initiating;
}

Gekko::PlayerType Gekko::Player::GetType()
{
	return _type;
}

Gekko::PlayerStatus Gekko::Player::GetStatus()
{
	return _status;
}
