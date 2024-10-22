#include "gekko_types.h"
#include "net.h"

#include <cassert>
#include <iostream>

Gekko::u32 Gekko::NetAddress::GetSize()
{
    return _size;
}

Gekko::NetAddress::NetAddress(void* data, u32 size)
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

void Gekko::NetAddress::Copy(NetAddress* other)
{
    if (!other) {
        return;
    }

    _size = other->_size;

    if (_data) {
        _data.reset();
    }

    _data = std::unique_ptr<u8[]>(new u8[_size]);
    // copy address data
    std::memcpy(_data.get(), other->GetAddress(), _size);
}

bool Gekko::NetAddress::Equals(NetAddress& other)
{
    return _size == other._size && std::memcmp(_data.get(), other._data.get(), _size) == 0;
}

Gekko::u8* Gekko::NetAddress::GetAddress()
{
    return _data.get();
}

#ifndef GEKKONET_NO_ASIO
Gekko::NonBlockingSocket::NonBlockingSocket(u16 port)
{
    _socket = std::make_unique<asio::ip::udp::socket>(_io_ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
    _socket->non_blocking(true);
}

std::vector<std::unique_ptr<Gekko::NetResult>> Gekko::NonBlockingSocket::ReceiveData()
{
    std::vector<std::unique_ptr<Gekko::NetResult>> results;

    while (true) {
        const u32 len = (u32)_socket->receive_from(asio::buffer(_buffer), _remote, 0, _ec);
        if (_ec && _ec != asio::error::would_block) {
            // std::cout << "receive failed: " << _ec.message() << std::endl;
            continue;
        } else if (!_ec) {
            std::string endpoint = ETOS(_remote);

            results.push_back(std::make_unique<NetResult>());
            auto& res = results.back();
            res->addr = NetAddress((void*)endpoint.c_str(), (u32)endpoint.size());

            res->data_len = len;
            res->data = std::unique_ptr<char[]>(new char[len]);

            std::memcpy(res->data.get(), _buffer, len);
        } else {
            break;
        }
    }

    return results;
}

void Gekko::NonBlockingSocket::SendData(NetAddress& addr, const char* data, int length)
{
    std::string address((char*)addr.GetAddress(), addr.GetSize());

    auto endpoint = STOE(address);

    _socket->send_to(asio::buffer(data, length), endpoint, 0, _ec);
    if (_ec) {
        std::cerr << "send failed: " << _ec.message() << std::endl;
    }
}
#endif 

