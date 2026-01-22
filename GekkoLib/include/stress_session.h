#pragma once

#include "session.h";

namespace Gekko {
    class StressSession : public GekkoSession {
    public:
        virtual void Init(GekkoConfig* config);

        virtual void SetLocalDelay(i32 player, u8 delay);

        virtual void SetNetAdapter(GekkoNetAdapter* adapter);

        virtual i32 AddActor(GekkoPlayerType type, GekkoNetAddress* addr);

        virtual void AddLocalInput(i32 player, void* input);

        virtual GekkoGameEvent** UpdateSession(i32* count);

        virtual GekkoSessionEvent** Events(i32* count);

        virtual f32 FramesAhead();

        virtual void NetworkStats(i32 player, GekkoNetworkStats* stats);

        virtual void NetworkPoll();
    };
}
