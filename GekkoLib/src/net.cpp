#include "gekko_types.h"
#include "net.h"

#include <cassert>
#include <iostream>

u32 Gekko::NetAddress::GetSize()
{
    return _size;
}

Gekko::NetAddress::NetAddress(void* data, u32 size)
{
    _size = size;
    _data = std::make_unique<u8[]>(_size);
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

    _data = std::make_unique<u8[]>(_size);
    // copy address data
    std::memcpy(_data.get(), other->GetAddress(), _size);
}

bool Gekko::NetAddress::Equals(NetAddress& other)
{
    return _size == other._size && std::memcmp(_data.get(), other._data.get(), _size) == 0;
}

u8* Gekko::NetAddress::GetAddress()
{
    return _data.get();
}

float Gekko::NetStats::CalculateJitter()
{
    if (rtt.size() < 2) {
        return 0.f;
    }

    float sum = 0.f;
    for (i32 i = 1; i < rtt.size(); ++i) {
        sum += std::abs((float)rtt[i] - (float)rtt[i - 1]);
    }

    float jitter = sum / (rtt.size() - 1);
    return jitter;
}

float Gekko::NetStats::CalculateAvgRTT()
{
    if (rtt.empty()) {
        return 0.f;
    }

    float sum = 0.f;
    for (i32 i = 0; i < rtt.size(); i++) {
        sum += rtt[i];
    }

    float avg_rtt = sum / rtt.size();
    return avg_rtt;
}

void Gekko::NetStats::AddRTT(u16 rtt_ms)
{
    rtt.push_back(rtt_ms);
    while (rtt.size() > RTT_HISTORY_SIZE) {
        rtt.erase(rtt.begin());
    }
}

void Gekko::NetStats::UpdateBandwidth()
{
    using namespace std::chrono;
    u64 now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

    if (last_bandwidth_update == 0) {
        last_bandwidth_update = now;
        return;
    }

    u64 elapsed = now - last_bandwidth_update;
    if (elapsed >= 1000) {
        kb_sent_per_sec = (bytes_sent_accum * 1000.f / elapsed) / 1024.f;
        kb_received_per_sec = (bytes_received_accum * 1000.f / elapsed) / 1024.f;
        bytes_sent_accum = 0;
        bytes_received_accum = 0;
        last_bandwidth_update = now;
    }
}

u32 Gekko::NetStats::LastRTT()
{
    if (rtt.empty()) {
        return 0;
    }

    return rtt.back();
}
