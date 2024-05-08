#include "backend.h"

Gekko::Player::Player(Handle phandle, PlayerType type, NetAddress* addr, u32 magic)
{
	handle = phandle;
	sync_num = 0;
	session_magic = magic;
	address.Copy(addr);

	_type = type;
	_stats = NetStats();
	_status = _type == LocalPlayer ? Connected : Initiating;
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
