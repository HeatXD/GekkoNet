#pragma once

#include <vector>

#include "session.h"
#include "backend.h"
#include "event.h"
#include "sync.h"
#include "storage.h"

namespace Gekko {

	class GameSession : public GekkoSession {
    public:
        GameSession();

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
		void Poll();

		bool AllActorsValid();

		void HandleReceivedInputs();

		void SendLocalInputs();

		u8 GetMinLocalDelay();

		bool IsSpectating();

		bool IsPlayingLocally();

        bool IsLockstepActive() const;

		void AddDisconnectedPlayerInputs();

		void SendSpectatorInputs();

		void HandleRollback(std::vector<GekkoGameEvent*>& ev);

		bool AddAdvanceEvent(std::vector<GekkoGameEvent*>& ev, bool rolling_back);

		void AddSaveEvent(std::vector<GekkoGameEvent*>& ev);

		void AddLoadEvent(std::vector<GekkoGameEvent*>& ev);

		void HandleSavingConfirmedFrame(std::vector<GekkoGameEvent*>& ev);

		void UpdateLocalFrameAdvantage();

        bool ShouldDelaySpectator();

        void SendSessionHealthCheck();

        void SendNetworkHealthCheck();

        void SessionIntegrityCheck();

	private:
		bool _started;

        bool _delay_spectator;

		Frame _last_saved_frame;

        Frame _last_sent_healthcheck;

		std::unique_ptr<u8[]> _disconnected_input;

		GekkoConfig _config;

		SyncSystem _sync;

        GekkoNetAdapter* _host;

		MessageSystem _msg;

		StateStorage _storage;

        GameEventBuffer _game_event_buffer;

        std::vector<GekkoGameEvent*> _current_game_events;
	};
}

