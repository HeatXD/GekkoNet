#pragma once

#include "gekkonet.h"
#include "gekko_types.h"
#include "sync.h"
#include "storage.h"

#include <vector>
#include <memory>

namespace Gekko {
    struct GameEventBuffer {
    public:
        void Init(u32 input_size);

        GekkoGameEvent* GetEvent(bool advance);

        void Reset();

    private:
        u32 _input_size = 0;

        u16 _index_others = 0;

        u16 _index_advance = 0;

        std::vector<std::unique_ptr<GekkoGameEvent>> _buffer_advance;

        std::vector<std::unique_ptr<GekkoGameEvent>> _buffer_others;

        std::vector<std::unique_ptr<u8[]>> _input_memory_buffer;
    };

    struct GameEventSystem {
    public:
        void Init(u32 input_size);

        bool AddAdvanceEvent(SyncSystem& sync, bool rolling_back);

        void AddSaveEvent(SyncSystem& sync, StateStorage& storage, Frame* last_saved_frame = nullptr);

        void AddLoadEvent(SyncSystem& sync, StateStorage& storage);

        std::vector<GekkoGameEvent*>& GetEvents();

        void Reset();

        void Clear();

        i32 Count();

        GekkoGameEvent** Data();

    private:
        std::vector<GekkoGameEvent*> _current_events;

        GameEventBuffer _event_buffer;
    };

    struct SessionEventBuffer {
    public:
        SessionEventBuffer();

        GekkoSessionEvent* GetEvent();

        void Reset();

    private:
        u16 _index;

        std::vector<std::unique_ptr<GekkoSessionEvent>> _buffer;
    };

    struct SessionEventSystem {
    public:
        void Reset();

        std::vector<GekkoSessionEvent*>& GetRecentEvents();

        void AddPlayerSyncingEvent(Handle handle, u8 sync, u8 max);

        void AddPlayerConnectedEvent(Handle handle);

        void AddPlayerDisconnectedEvent(Handle handle);

        void AddSessionStartedEvent();

        void AddSpectatorPausedEvent();

        void AddSpectatorUnpausedEvent();

        void AddDesyncDetectedEvent(Frame frame, Handle remote, u32 check_local, u32 check_remote);

    private:
        void AddEvent(GekkoSessionEvent* ev);

    private:
        std::vector<GekkoSessionEvent*> _events;

        SessionEventBuffer _event_buffer;
    };
}
