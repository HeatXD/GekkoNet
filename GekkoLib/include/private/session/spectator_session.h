#pragma once

#include "gekkonet.h"
#include "gekko_types.h"
#include "session.h"
#include "backend.h"
#include "event.h"
#include "replay.h"
#include "sync.h"

#include <vector>

namespace Gekko {

	class SpectatorSession : public GekkoSession {
    public:
        SpectatorSession();

        void Init(GekkoConfig* config) override;

        void SetNetAdapter(GekkoNetAdapter* adapter) override;

        i32 AddActor(GekkoPlayerType type, GekkoNetAddress* addr) override;

        bool DisconnectActor(i32 actor) override;

        void SetDisconnectTimeout(u32 timeout) override;

        GekkoGameEvent** UpdateSession(i32* count) override;

        GekkoSessionEvent** Events(i32* count) override;

        void NetworkStats(i32 player, GekkoNetworkStats* stats) override;

        void NetworkPoll() override;

        bool StartRecording(bool save_initial_state, bool disable_compression) override;

        const u8* StopRecording(u32& length) override;

	private:
		void Poll();

		bool AllActorsValid();

		void HandleReceivedInputs();

        bool ShouldDelaySpectator();

        void UpdateRecording();

	private:
		bool _started;

        bool _delay_spectator;

		Frame _last_saved_frame;

		GekkoConfig _config;

		SyncSystem _sync;

        GekkoNetAdapter* _host;

		MessageSystem _msg;

        GameEventSystem _game_events;

        ReplaySystem _replay;

        bool _spectator_state_pending;

        Frame _spectator_state_frame;

        std::vector<u8> _spectator_state;
	};
}
