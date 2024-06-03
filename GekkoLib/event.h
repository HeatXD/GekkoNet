#pragma once

#include "gekko_types.h"

#include <vector>
#include <memory>

namespace Gekko {

    enum GameEventType {
        EmptyGameEvent= -1,
        AdvanceEvent,
        SaveEvent,
        LoadEvent
    };

    enum SessionEventType {
        EmptySessionEvent = -1,
        PlayerSyncing,
        PlayerConnected,
        PlayerDisconnected,
        SessionStarted,
        SpectatorPaused,
        SpectatorUnpaused,
        DesyncDetected,
    };

	struct GameEvent {
        ~GameEvent();

        GameEventType type;

        union EventData {
            // events 
            struct Advance {
                Frame frame;
                u32 input_len;
                u8* inputs;
            } adv;
            struct Save {
                Frame frame;
                u32* checksum;
                u32* state_len;
                u8* state;
            } save;
            struct Load {
                Frame frame;
                u32* state_len;
                u8* state;
            } load;
        } data;
	};

    struct GameEventBuffer {

        void Init(u32 input_size);

        GameEvent* GetEvent(bool advance);

        void Reset();

    private:
        u32 _input_size;

        u16 _index_others;

        u16 _index_advance;

        std::vector<std::unique_ptr<GameEvent>> _buffer_advance;

        std::vector<std::unique_ptr<GameEvent>> _buffer_others;
    };

    struct SessionEvent {
        ~SessionEvent();

        SessionEventType type;

        union Data {
            struct Syncing {
                Handle handle;
                u8 current;
                u8 max;
            } syncing;
            struct Connected {
                Handle handle;
            } connected;
            struct Disconnected {
                Handle handle;
            } disconnected;
            struct Desynced {
                u32 local_checksum;
                u32 num_remote_handles;
                Handle* remote_handles;
                u32* remote_checksums;
            } desynced;
        } data;
    };

    struct SessionEventBuffer {
    public:
        SessionEventBuffer();

        SessionEvent* GetEvent();

        void Reset();

    private:
        u16 _index;

        std::vector<std::unique_ptr<SessionEvent>> _buffer;
    };

    struct SessionEventSystem {
    public:
        void Reset();

        std::vector<SessionEvent*> GetRecentEvents();

        void AddPlayerSyncingEvent(Handle handle, u8 sync, u8 max);

        void AddPlayerConnectedEvent(Handle handle);

        void AddPlayerDisconnectedEvent(Handle handle);

        void AddSessionStartedEvent();

        void AddSpectatorPausedEvent();

        void AddSpectatorUnpausedEvent();

    public:
        SessionEventSystem(SessionEventSystem const&) = delete;
        SessionEventSystem& operator=(SessionEventSystem const&) = delete;

        static std::shared_ptr<Gekko::SessionEventSystem> Get();

    private:
        SessionEventSystem() {};

        void AddEvent(SessionEvent* ev);

    private:
        std::vector<SessionEvent*> _events;

        SessionEventBuffer _event_buffer;
    };

}
