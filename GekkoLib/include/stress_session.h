#pragma once

#include "session.h"

namespace Gekko {
    class StressSession : public GekkoSession {
    public:
        void Init(GekkoConfig* config) override;

        void SetLocalDelay(i32 player, u8 delay) override;

        void SetNetAdapter(GekkoNetAdapter* adapter) override;

        i32 AddActor(GekkoPlayerType type, GekkoNetAddress* addr) override;

        void AddLocalInput(i32 player, void* input) override;

        GekkoGameEvent** UpdateSession(i32* count) override;

        GekkoSessionEvent** Events(i32* count) override;

        f32 FramesAhead() override;

        void NetworkStats(i32 player, GekkoNetworkStats* stats) override;

        void NetworkPoll() override;

    private:
    };
}
