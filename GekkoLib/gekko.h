#pragma once

#include <vector>

#include "backend.h"
#include "gekko_types.h"
#include "event.h"
#include "sync.h"

namespace Gekko {
	struct Config {
		u8 num_players;
		u8 max_spectators;
		u8 input_prediction_window;
		u32 input_size;
	};

	class Session {
	public:
		static void Test();

		Session();

		void Init(Config& conf);

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

	private:
		bool _started;

		u8 _num_players;
		u8 _max_spectators;
		u8 _input_prediction_window;

		u32 _input_size;

		SyncSystem _sync;

		MessageSystem _msg;

		NetAdapter* _host;
	};
}