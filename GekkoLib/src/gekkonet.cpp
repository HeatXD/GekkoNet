#include "gekkonet.h"

namespace GekkoNet {

    struct NetAddress::Impl {
    public:
        std::unique_ptr<Gekko::NetAddress> address;
    };

    NetAddress::NetAddress() {
        _ref = std::make_unique<Impl>();
        _ref->address = std::make_unique<Gekko::NetAddress>();
    }

    NetAddress::NetAddress(void* data, uint32_t size) {
        _ref = std::make_unique<Impl>();
        _ref->address = std::make_unique<Gekko::NetAddress>(data, size);
    }

    uint8_t* NetAddress::GetAddress() {
        return _ref->address->GetAddress();
    }

    uint32_t NetAddress::GetSize() {
        return _ref->address->GetSize();
    }

    void NetAddress::Copy(NetAddress* other) {
        _ref->address->Copy(other->_ref->address.get());
    }

    bool NetAddress::Equals(NetAddress& other) {
        return _ref->address->Equals(*other._ref->address.get());
    }

    struct NetAdapter::Impl {
    public:
        std::unique_ptr<Gekko::NetAdapter> adapter;
    };

    class Session::Impl {
    public:
        std::unique_ptr<Gekko::Session> session;
    };

    Session::Session() {
        _ref = std::make_unique<Impl>();
        _ref->session = std::make_unique<Gekko::Session>();
    }

    void Session::Init(Gekko::Config& config) {
        _ref->session->Init(config);
    }

    void Session::SetLocalDelay(int player, unsigned char delay) {
        _ref->session->SetLocalDelay(player, delay);
    }

    void Session::SetNetAdapter(NetAdapter* adapter) {
        // todo
    }

    int Session::AddActor(Gekko::PlayerType type, NetAddress* addr) {
        if (!addr) {
            return _ref->session->AddActor(type, nullptr);
        }

        auto address = Gekko::NetAddress(addr->GetAddress(), addr->GetSize());
        return _ref->session->AddActor(type, &address);
    }

    void Session::AddLocalInput(int player, void* input) {
        _ref->session->AddLocalInput(player, input);
    }

    Gekko::GameEvent** Session::UpdateSession(int* ev_len) {
        auto data = _ref->session->UpdateSession();
        *ev_len = (int)data.size();
        return data.data();
    }

    Gekko::SessionEvent** Session::Events(int* ev_len) {
        auto data = _ref->session->Events();
        *ev_len = (int)data.size();
        return data.data();
    }

    float Session::FramesAhead() {
        return _ref->session->FramesAhead();
    }
};
