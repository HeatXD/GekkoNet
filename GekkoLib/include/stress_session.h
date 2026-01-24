#pragma once

#include <vector>

#include "session.h"
#include "storage.h"
#include "sync.h"
#include "event.h"
#include "backend.h"

namespace Gekko {
    class StressSession : public GekkoSession {
    public:
        StressSession();

        void Init(GekkoConfig* config) override;

        void SetLocalDelay(i32 player, u8 delay) override;

        void SetNetAdapter(GekkoNetAdapter* adapter) override;

        i32 AddActor(GekkoPlayerType type, GekkoNetAddress* addr) override;

        void AddLocalInput(i32 player, void* input) override;

        GekkoGameEvent** UpdateSession(i32* count) override;

        GekkoSessionEvent** Events(i32* count) override;

        f32 FramesAhead() override;

        void NetworkStats(i32 player, GekkoNetworkStats* stats) override;

        void NetworkPoll() override;

    private:
        void HandleRollback(std::vector<GekkoGameEvent*>& ev);

        void CheckForDesyncs(Frame check_frame);

        bool AddAdvanceEvent(std::vector<GekkoGameEvent*>& ev, bool rolling_back);

        void AddSaveEvent(std::vector<GekkoGameEvent*>& ev);

        void AddLoadEvent(std::vector<GekkoGameEvent*>& ev);

    private:
        GekkoConfig _config;

        SyncSystem _sync;

        StateStorage _storage;

        GameEventBuffer _game_event_buffer;

        SessionEventSystem _session_events;

        std::vector<GekkoGameEvent*> _current_game_events;

        std::vector<Player> _locals;

        u32 _check_distance;

        std::map<Frame, u32> _checksum_history;
    };
}
