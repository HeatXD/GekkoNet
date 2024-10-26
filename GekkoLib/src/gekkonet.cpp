#include "gekkonet.h"
#include "gekko.h"

bool gekko_create(GekkoSession** session)
{
    if (!session) {
        *session = new Gekko::Session();
        return true;
    }

    return false;
}

bool gekko_destroy(GekkoSession* session)
{
    if (session) {
        delete session;
        return true;
    }

    return false;
}

void gekko_start(GekkoSession* session, GekkoConfig* config)
{
    session->Init(config);
}

void gekko_net_adapter_set(GekkoSession* session, GekkoNetAdapter* adapter)
{
    session->SetNetAdapter(adapter);
}

int gekko_add_actor(GekkoSession* session, GekkoPlayerType player_type, GekkoNetAddress* addr)
{
    return session->AddActor(player_type, !addr ? nullptr : addr);
}

void gekko_set_local_delay(GekkoSession* session, int player, unsigned char delay)
{
    session->SetLocalDelay(player, delay);
}

void gekko_add_local_input(GekkoSession* session, int player, void* input)
{
    session->AddLocalInput(player, input);
}

GekkoGameEvent** gekko_update_session(GekkoSession* session, int* count)
{
    return session->UpdateSession(count);
}

GekkoSessionEvent** gekko_session_events(GekkoSession* session, int* count)
{
    return session->Events(count);
}

float gekko_frames_ahead(GekkoSession* session)
{
    return session->FramesAhead();
}
