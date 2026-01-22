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

        virtual void Init(GekkoConfig* config);

        virtual void SetLocalDelay(i32 player, u8 delay);

        virtual void SetNetAdapter(GekkoNetAdapter* adapter);

        virtual i32 AddActor(GekkoPlayerType type, GekkoNetAddress* addr);

        virtual void AddLocalInput(i32 player, void* input);

        virtual GekkoGameEvent** UpdateSession(i32* count);

        virtual GekkoSessionEvent** Events(i32* count);

        virtual f32 FramesAhead();

        virtual void NetworkStats(i32 player, GekkoNetworkStats* stats);

        virtual void NetworkPoll();

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

