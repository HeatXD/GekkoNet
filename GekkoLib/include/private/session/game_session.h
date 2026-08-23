#pragma once

#include <memory>

#include "gekkonet.h"
#include "gekko_types.h"
#include "session.h"
#include "backend.h"
#include "event.h"
#include "replay.h"
#include "sync.h"
#include "storage.h"

namespace Gekko {

	class GameSession : public GekkoSession {
    public:
        GameSession();

        void Init(GekkoConfig* config) override;

        void SetLocalDelay(i32 player, u8 delay) override;

        void SetRunahead(u8 runahead) override;

        void SetNetAdapter(GekkoNetAdapter* adapter) override;

        i32 AddActor(GekkoPlayerType type, GekkoNetAddress* addr) override;

        bool DisconnectActor(i32 actor) override;

        void SetDisconnectTimeout(u32 timeout) override;

        void AddLocalInput(i32 player, void* input) override;

        GekkoGameEvent** UpdateSession(i32* count) override;

        GekkoSessionEvent** Events(i32* count) override;

        f32 FramesAhead() override;

        void NetworkStats(i32 player, GekkoNetworkStats* stats) override;

        void NetworkPoll() override;

        bool StartRecording(bool save_initial_state, bool disable_compression) override;

        const u8* StopRecording(u32& length) override;

	private:
		void Poll();

        void UpdateRecording();

        void RecordInitialState();

		bool AllActorsValid();

		void HandleReceivedInputs();

		void SendLocalInputs();

		u8 GetMinLocalDelay();

		bool IsPlayingLocally();

        bool IsLockstepActive() const;

		void AddDisconnectedPlayerInputs();

		void SendSpectatorInputs();

        void PrepareSpectatorStates();

        void CaptureSpectatorState();

		void HandleRollback();

		void HandleSavingConfirmedFrame();

		void HandleRunahead();

		void RewindRunahead();

		bool RollbackPending();

		bool ConfirmedSaveDue();

		Frame GetConfirmedFrame();

		bool ShouldStallAdvance();

        void SendSessionHealthCheck();

        void SendNetworkHealthCheck();

        void SessionIntegrityCheck();

	private:
		bool _started;

		Frame _last_saved_frame;

        Frame _last_sent_healthcheck;

		Frame _runahead_start_frame;

		u8 _runahead_frames;

		std::unique_ptr<u8[]> _disconnected_input;

		GekkoConfig _config;

		SyncSystem _sync;

        GekkoNetAdapter* _host;

		MessageSystem _msg;

		StateStorage _storage;

        GameEventSystem _game_events;

        ReplaySystem _replay;

        StateEntry _spectator_state;
	};
}
