#pragma once

#include <vector>

#include "backend.h"
#include "gekko_types.h"
#include "event.h"
#include "sync.h"
#include "storage.h"

namespace Gekko {
	struct Config {
		u8 num_players = 0;
		u8 max_spectators = 0;
		u8 input_prediction_window = 0;
		u32 input_size = 0;
		u32 state_size = 0;
		bool limited_saving = false;
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

	private:
		void Poll();

		bool AllPlayersValid();

		void HandleReceivedInputs();

		void SendLocalInputs();

		u8 GetMinLocalDelay();

		void AddDisconnectedPlayerInputs();

		void SendSpectatorInputs();

		void HandleRollback(std::vector<Event>& ev);

		bool AddAdvanceEvent(std::vector<Event>& ev);

		void AddSaveEvent(std::vector<Event>& ev);

		void AddLoadEvent(std::vector<Event>& ev);

		void HandleSavingConfirmedFrame(std::vector<Event>& ev, Frame confirmed_frame, Frame current);

	private:
		bool _started;

		Frame _last_saved_frame;

		Config _config;

		SyncSystem _sync;

		MessageSystem _msg;

		StateStorage _storage;

		NetAdapter* _host;
	};
}