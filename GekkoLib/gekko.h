#pragma once

#include <vector>

#include "backend.h"
#include "gekko_types.h"
#include "event.h"
#include "sync.h"
#include "storage.h"

namespace Gekko {
	struct Config {
        static const u8 MAX_SPECTATOR_DELAY = InputBuffer::BUFF_SIZE * 0.75; // max delay in frames

		u8 num_players = 0;
		u8 max_spectators = 0;
		u8 input_prediction_window = 0;
        u8 spectator_delay = 0;
		u32 input_size = 0;
		u32 state_size = 0;
        bool limited_saving = false;
        bool post_sync_joining = false;
	};

	class Session {
	public:
		static void Test();

		Session();

		void Init(Config& config);

		void SetLocalDelay(Handle player, u8 delay);

		void SetNetAdapter(NetAdapter* adapter);

		Handle AddActor(PlayerType type, NetAddress* addr = nullptr);

		void AddLocalInput(Handle player, void* input);

		std::vector<Event> UpdateSession();

		f32 FramesAhead();

	private:
		void Poll();

		bool AllPlayersValid();

		void HandleReceivedInputs();

		void SendLocalInputs();

		u8 GetMinLocalDelay();

		bool IsSpectating();

		bool IsPlayingLocally();

		void AddDisconnectedPlayerInputs();

		void SendSpectatorInputs();

		void HandleRollback(std::vector<Event>& ev);

		bool AddAdvanceEvent(std::vector<Event>& ev);

		void AddSaveEvent(std::vector<Event>& ev);

		void AddLoadEvent(std::vector<Event>& ev);

		void HandleSavingConfirmedFrame(std::vector<Event>& ev);

		void UpdateLocalFrameAdvantage();

        bool ShouldDelaySpectator();

	private:
		bool _started;

        bool _delay_spectator;

		Frame _last_saved_frame;

		std::unique_ptr<u8[]> _disconnected_input;

		Config _config;

		SyncSystem _sync;

		MessageSystem _msg;

		StateStorage _storage;

		NetAdapter* _host;
	};
}
