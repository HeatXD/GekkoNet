#include "stress_session.h"

void Gekko::StressSession::Init(GekkoConfig* config)
{
}

void Gekko::StressSession::SetLocalDelay(i32 player, u8 delay)
{
}

void Gekko::StressSession::SetNetAdapter(GekkoNetAdapter* adapter)
{
}

i32 Gekko::StressSession::AddActor(GekkoPlayerType type, GekkoNetAddress* addr)
{
    return i32();
}

void Gekko::StressSession::AddLocalInput(i32 player, void* input)
{
}

GekkoGameEvent** Gekko::StressSession::UpdateSession(i32* count)
{
    return nullptr;
}

GekkoSessionEvent** Gekko::StressSession::Events(i32* count)
{
    return nullptr;
}

f32 Gekko::StressSession::FramesAhead()
{
    return 0.f;
}

void Gekko::StressSession::NetworkStats(i32 player, GekkoNetworkStats* stats)
{
}

void Gekko::StressSession::NetworkPoll()
{
}
