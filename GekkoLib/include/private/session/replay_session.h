#pragma once

#include <memory>

#include "gekkonet.h"
#include "gekko_types.h"
#include "session.h"
#include "event.h"
#include "replay.h"
#include "sync.h"

namespace Gekko {

    class ReplaySession : public GekkoSession {
    public:
        ReplaySession();

        void Init(GekkoConfig* config) override;

        GekkoGameEvent** UpdateSession(i32* count) override;

        GekkoSessionEvent** Events(i32* count) override;

        bool LoadReplay(const u8* replay_data, u32 length) override;

        const u8* ReplayUserData(u32& length) override;

    private:
        void AddInitialStateLoad();

        bool AddNextReplayInputs();

        void AddChecksumSave(Frame frame);

        void VerifyChecksum();

    private:
        bool _started;

        bool _finished;

        bool _desynced;

        bool _checksum_pending;

        u32 _recorded_checksum;

        StateEntry _checksum_state;

        GekkoConfig _config;

        SyncSystem _sync;

        GameEventSystem _game_events;

        SessionEventSystem _session_events;

        ReplaySystem _replay;

        std::unique_ptr<u8[]> _inputs;
    };
}
