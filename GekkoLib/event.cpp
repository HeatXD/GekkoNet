#include "event.h"
#include <cassert>

void Gekko::GameEventBuffer::Init(u32 input_size)
{
    _input_size = input_size;
    _index_advance = 0;
    _index_others = 0;
}

void Gekko::GameEventBuffer::Reset()
{
    _index_advance = 0;
    _index_others = 0;

    // clear the inputs
    for (auto& ev : _buffer_advance){
        if (ev->type == EmptyGameEvent) {
            break;
        }
        // reuse the input buffer rather than destroying it.
        // set event to empty to be used again.
        ev->type = EmptyGameEvent;
    }

    for (auto& ev : _buffer_others) {
        if (ev->type == EmptyGameEvent) {
            break;
        }
        // set event to empty to be used again.
        ev->type = EmptyGameEvent;
    }
}

Gekko::GameEvent* Gekko::GameEventBuffer::GetEvent(bool advance)
{
    auto& buff = advance ? _buffer_advance : _buffer_others;
    u16& idx = advance ? _index_advance : _index_others;

    idx++;

    if (buff.size() < idx) {
        buff.push_back(std::make_unique<GameEvent>());

        if (advance) {
            buff.back()->data.adv.input_len = _input_size;
            buff.back()->data.adv.inputs = (u8*)std::malloc(_input_size);
        }
    }

    assert(idx != 0);

    return buff[idx - 1].get();
}

Gekko::GameEvent::~GameEvent()
{
    // cleanup inputs automatically
    if (type == AdvanceEvent && data.adv.inputs) {
        std::free(data.adv.inputs);
        data.adv.inputs = nullptr;
    }
}

Gekko::SessionEventBuffer::SessionEventBuffer()
{
    _index = 0;
}

Gekko::SessionEvent* Gekko::SessionEventBuffer::GetEvent()
{
    _index++;

    if(_buffer.size() < _index) {
        _buffer.push_back(std::make_unique<SessionEvent>());
    }

    assert(_index != 0);

    return _buffer[_index - 1].get();
}

void Gekko::SessionEventBuffer::Reset()
{
    _index = 0;

    for (auto& ev : _buffer) {
        if (ev->type == EmptySessionEvent) {
            break;
        }

        ev->type = EmptySessionEvent;
    }
}

void Gekko::SessionEventSystem::Reset()
{
    _event_buffer.Reset();
    _events.clear();
}

void Gekko::SessionEventSystem::AddEvent(SessionEvent* ev)
{
    _events.push_back(ev);
}

std::vector<Gekko::SessionEvent*> Gekko::SessionEventSystem::GetRecentEvents()
{
    return _events;
}

void Gekko::SessionEventSystem::AddPlayerSyncingEvent(Handle handle, u8 sync, u8 max)
{
    auto ev = _event_buffer.GetEvent();
    ev->type = PlayerSyncing;
    ev->data.syncing.handle = handle;
    ev->data.syncing.current = sync;
    ev->data.syncing.max = max;
    AddEvent(ev);
}

void Gekko::SessionEventSystem::AddPlayerConnectedEvent(Handle handle)
{
    auto ev = _event_buffer.GetEvent();
    ev->type = PlayerConnected;
    ev->data.connected.handle = handle;
    AddEvent(ev);
}

void Gekko::SessionEventSystem::AddPlayerDisconnectedEvent(Handle handle)
{
    auto ev = _event_buffer.GetEvent();
    ev->type = PlayerDisconnected;
    ev->data.disconnected.handle = handle;
    AddEvent(ev);
}

void Gekko::SessionEventSystem::AddSessionStartedEvent()
{
    auto ev = _event_buffer.GetEvent();
    ev->type = SessionStarted;
    AddEvent(ev);
}

void Gekko::SessionEventSystem::AddSpectatorPausedEvent()
{
    auto ev = _event_buffer.GetEvent();
    ev->type = SpectatorPaused;
    AddEvent(ev);
}

void Gekko::SessionEventSystem::AddSpectatorUnpausedEvent()
{
    auto ev = _event_buffer.GetEvent();
    ev->type = SpectatorUnpaused;
    AddEvent(ev);
}

void Gekko::SessionEventSystem::AddDesyncDetectedEvent(Frame frame, Handle remote, u32 check_local, u32 check_remote)
{
    auto ev = _event_buffer.GetEvent();
    ev->type = DesyncDetected;
    ev->data.desynced.frame = frame;
    ev->data.desynced.remote_handle = remote;
    ev->data.desynced.local_checksum = check_local;
    ev->data.desynced.remote_checksum = check_remote;
    AddEvent(ev);
}
