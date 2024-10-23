#pragma once

#include "gekko.h"

// PIMPL is a save way to expose api
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

// Public Facing GekkoNet API
namespace GekkoNet {
    struct GEKKONET_API NetAdapter {
    private:
        struct Impl;
        std::unique_ptr<Impl> _ref;
    };

    struct GEKKONET_API NetAddress {
    public:
        NetAddress();
        NetAddress(void* data, uint32_t size);

        uint8_t* GetAddress();
        uint32_t GetSize();

        void Copy(NetAddress* other);
        bool Equals(NetAddress& other);

    private:
        struct Impl;
        std::unique_ptr<Impl> _ref;
    };

    class GEKKONET_API Session {
    public:
        Session();

        void Init(Gekko::Config& config);

        void SetLocalDelay(int player, unsigned char delay);
        
        void SetNetAdapter(NetAdapter* adapter);

        int AddActor(Gekko::PlayerType type, NetAddress* addr = nullptr);

        void AddLocalInput(int player, void* input);

        Gekko::GameEvent** UpdateSession(int* ev_len);

        Gekko::SessionEvent** Events(int* ev_len);

        float FramesAhead();

    private:
        class Impl;
        std::unique_ptr<Impl> _ref;
    };
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
