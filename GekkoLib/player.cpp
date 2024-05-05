#include "backend.h"

Gekko::Player::Player(Handle phandle, PlayerType type, NetAddress* addr, u32 magic)
{
	handle = phandle;

	session_magic = magic;
	address.Copy(addr);

	sync_num = 0;

	_type = type;

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

void Gekko::Player::SetStatus(PlayerStatus type)
{
	_status = type;
}
