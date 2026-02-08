#pragma once

#include <vector>
#include <map>
#include <memory>

#include "gekkonet.h"
#include "gekko_types.h"
#include "backend.h"
#include "event.h"
#include "sync.h"
#include "storage.h"

// define GekkoSession internally
struct GekkoSession {
    virtual void Init(GekkoConfig* config) = 0;
    virtual void SetLocalDelay(i32 player, u8 delay) = 0;
    virtual void SetNetAdapter(GekkoNetAdapter* adapter) = 0;
    virtual i32 AddActor(GekkoPlayerType type, GekkoNetAddress* addr) = 0;
    virtual void AddLocalInput(i32 player, void* input) = 0;
    virtual GekkoGameEvent** UpdateSession(i32* count) = 0;
    virtual GekkoSessionEvent** Events(i32* count) = 0;
    virtual f32 FramesAhead() = 0;
    virtual void NetworkStats(i32 player, GekkoNetworkStats* stats) = 0;
    virtual void NetworkPoll() = 0;
    virtual ~GekkoSession() = default;
};

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

		bool IsPlayingLocally();

        bool IsLockstepActive() const;

		void AddDisconnectedPlayerInputs();

		void SendSpectatorInputs();

		void HandleRollback();

		void HandleSavingConfirmedFrame();

		void UpdateLocalFrameAdvantage();

        void SendSessionHealthCheck();

        void SendNetworkHealthCheck();

        void SessionIntegrityCheck();

	private:
		bool _started;

		Frame _last_saved_frame;

        Frame _last_sent_healthcheck;

		std::unique_ptr<u8[]> _disconnected_input;

		GekkoConfig _config;

		SyncSystem _sync;

        GekkoNetAdapter* _host;

		MessageSystem _msg;

		StateStorage _storage;

        GameEventSystem _game_events;
	};

	class SpectatorSession : public GekkoSession {
    public:
        SpectatorSession();

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

        bool ShouldDelaySpectator();

	private:
		bool _started;

        bool _delay_spectator;

		Frame _last_saved_frame;

		GekkoConfig _config;

		SyncSystem _sync;

        GekkoNetAdapter* _host;

		MessageSystem _msg;

        GameEventSystem _game_events;
	};

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
        void HandleRollback();

        void CheckForDesyncs(Frame check_frame);

    private:
        GekkoConfig _config;

        SyncSystem _sync;

        StateStorage _storage;

        SessionEventSystem _session_events;

        GameEventSystem _game_events;

        std::vector<Player> _locals;

        u32 _check_distance;

        std::map<Frame, u32> _checksum_history;
    };
}
