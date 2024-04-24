#include "backend.h"


Gekko::u32 Gekko::NetAddress::GetSize()
{
	return _size;
}

Gekko::NetAddress::NetAddress(u8* data, u32 size)
{
	_size = size;
	_data = std::unique_ptr<u8[]>(new u8[_size]);
	// copy address data
	std::memcpy(_data.get(), data, _size);
}

Gekko::NetAddress::NetAddress()
{
	_size = 0;
	_data = nullptr;
}

void Gekko::NetAddress::Copy(NetAddress& other)
{
	_size = other._size;

	if (_data) 
		_data.reset();

	_data = std::unique_ptr<u8[]>(new u8[_size]);
	// copy address data
	std::memcpy(_data.get(), other.GetAddress(), _size);
}

Gekko::u8* Gekko::NetAddress::GetAddress()
{
	return _data.get();
}
