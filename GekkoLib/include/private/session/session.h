#pragma once

#include "gekkonet.h"
#include "gekko_types.h"

// define GekkoSession internally
struct GekkoSession {
    virtual void Init(GekkoConfig* config) = 0;
    virtual void SetLocalDelay(i32 player, u8 delay) {}
    virtual void SetRunahead(u8 runahead) {}
    virtual void SetNetAdapter(GekkoNetAdapter* adapter) {}
    virtual i32 AddActor(GekkoPlayerType type, GekkoNetAddress* addr) { return -1; }
    virtual bool DisconnectActor(i32 actor) { return false; }
    virtual void SetDisconnectTimeout(u32 timeout) {}
    virtual void AddLocalInput(i32 player, void* input) {}
    virtual GekkoGameEvent** UpdateSession(i32* count) = 0;
    virtual GekkoSessionEvent** Events(i32* count) = 0;
    virtual f32 FramesAhead() { return 0.f; }
    virtual void NetworkStats(i32 player, GekkoNetworkStats* stats) {}
    virtual void NetworkPoll() {}
    virtual bool StartRecording(bool save_initial_state, bool disable_compression) { return false; }
    virtual const u8* StopRecording(u32& length) { return nullptr; }
    virtual const u8* PeekRecording(u32& length) { return nullptr; }
    virtual bool RecordChecksums(u32 interval) { return false; }
    virtual bool SetReplayUserData(const u8* data, u32 length) { return false; }
    virtual bool LoadReplay(const u8* replay_data, u32 length) { return false; }
    virtual const u8* ReplayUserData(u32& length) { length = 0; return nullptr; }
    virtual ~GekkoSession() = default;
};
