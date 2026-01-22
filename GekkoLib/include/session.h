#pragma once

#include "gekkonet.h"
#include "gekko_types.h"

// define GekkoSession internally
struct GekkoSession {
    virtual void Init(GekkoConfig* config) = 0;
    virtual void SetLocalDelay(i32 player, u8 delay) = 0;
    virtual void SetNetAdapter(GekkoNetAdapter* adapter) = 0;
    virtual i32 AddActor(GekkoPlayerType type, GekkoNetAddress* addr) = 0;
    virtual void AddLocalInput(i32 player, void* input) = 0;
    virtual GekkoGameEvent** UpdateSession(i32* count) = 0;
    virtual GekkoSessionEvent** Events(i32* count) = 0;
    virtual f32 FramesAhead() = 0;
    virtual void NetworkStats(i32 player, GekkoNetworkStats* stats) = 0;
    virtual void NetworkPoll() = 0;
    virtual ~GekkoSession() = default;
};
