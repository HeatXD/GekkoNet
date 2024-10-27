#include "gekkonet.h"
#include "gekko.h"

#ifndef GEKKONET_NO_ASIO

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif // _WIN32

#define ASIO_STANDALONE 

#include "asio/asio.hpp"

#endif // GEKKONET_NO_ASIO

bool gekko_create(GekkoSession** session)
{
    if (*session) {
        return false;
    }

    *session = new Gekko::Session();
    return true;
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

#ifndef GEKKONET_NO_ASIO

static void asio_send(GekkoNetAddress* addr, const char* data, int length) {
}

static GekkoNetResult** asio_receive(int* length) {
}

static void asio_free(void* data_ptr) {
    delete data_ptr;
}

static GekkoNetAdapter default_sock {
    asio_send,
    asio_receive,
    asio_free
};

GekkoNetAdapter* gekko_default_adapter() {
    return &default_sock;
}

#endif
