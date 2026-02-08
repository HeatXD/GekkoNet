#include "event.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

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

GekkoGameEvent* Gekko::GameEventBuffer::GetEvent(bool advance)
{
    auto& buff = advance ? _buffer_advance : _buffer_others;
    u16& idx = advance ? _index_advance : _index_others;

    idx++;

    if (buff.size() < idx) {
        buff.push_back(std::make_unique<GekkoGameEvent>());

        if (advance) {
            buff.back()->data.adv.input_len = _input_size;
            // add more input space when needed
            if (_input_memory_buffer.size() < idx) {
                _input_memory_buffer.push_back(std::make_unique<u8[]>(_input_size));
            }
            buff.back()->data.adv.inputs = _input_memory_buffer.back().get();
        }
    }

    assert(idx != 0);

    return buff[idx - 1].get();
}

Gekko::SessionEventBuffer::SessionEventBuffer()
{
    _index = 0;
}

GekkoSessionEvent* Gekko::SessionEventBuffer::GetEvent()
{
    _index++;

    if(_buffer.size() < _index) {
        _buffer.push_back(std::make_unique<GekkoSessionEvent>());
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

void Gekko::SessionEventSystem::AddEvent(GekkoSessionEvent* ev)
{
    _events.push_back(ev);
}

std::vector<GekkoSessionEvent*>& Gekko::SessionEventSystem::GetRecentEvents()
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

void Gekko::GameEventSystem::Init(u32 input_size) {
    _event_buffer.Init(input_size);
    _event_buffer.Reset();
    _current_events.clear();
}

bool Gekko::GameEventSystem::AddAdvanceEvent(SyncSystem& sync, bool rolling_back)
{
    Frame frame = GameInput::NULL_FRAME;
    std::unique_ptr<u8[]> inputs;
    if (!sync.GetCurrentInputs(inputs, frame)) {
        return false;
    }

    _current_events.push_back(_event_buffer.GetEvent(true));

    auto event = _current_events.back();

    event->type = AdvanceEvent;
    event->data.adv.frame = frame;
    event->data.adv.rolling_back = rolling_back;

    if (event->data.adv.inputs) {
        std::memcpy(event->data.adv.inputs, inputs.get(), event->data.adv.input_len);
    }

    return true;
}

void Gekko::GameEventSystem::AddSaveEvent(SyncSystem& sync, StateStorage& storage, Frame* last_saved_frame)
{
    const Frame frame_to_save = sync.GetCurrentFrame();

    auto state = storage.GetState(frame_to_save);

    state->frame = frame_to_save;

    _current_events.push_back(_event_buffer.GetEvent(false));

    auto event = _current_events.back();
    event->type = SaveEvent;

    event->data.save.frame = frame_to_save;
    event->data.save.state = state->state.get();
    event->data.save.checksum = &state->checksum;
    event->data.save.state_len = &state->state_len;

    if (last_saved_frame) {
        *last_saved_frame = frame_to_save;
    }
}

void Gekko::GameEventSystem::AddLoadEvent(SyncSystem& sync, StateStorage& storage)
{
    const Frame frame_to_load = sync.GetCurrentFrame();

    auto state = storage.GetState(frame_to_load);

    _current_events.push_back(_event_buffer.GetEvent(false));

    auto event = _current_events.back();
    event->type = LoadEvent;

    event->data.load.frame = frame_to_load;
    event->data.load.state = state->state.get();
    event->data.load.state_len = state->state_len;
}

std::vector<GekkoGameEvent*>& Gekko::GameEventSystem::GetEvents()
{
    return _current_events;
}

void Gekko::GameEventSystem::Reset()
{
    _event_buffer.Reset();
}

void Gekko::GameEventSystem::Clear()
{
    _current_events.clear();
}

i32 Gekko::GameEventSystem::Count()
{
    return (i32)_current_events.size();
}

GekkoGameEvent** Gekko::GameEventSystem::Data()
{
    return _current_events.data();
}
