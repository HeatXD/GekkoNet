#pragma once

#include <vector>

#include "backend.h"
#include "gekko_types.h"
#include "event.h"
#include "sync.h"

namespace Gekko {
	class Session {
	public:
		static void Test();

		Session();

		void Init(u8 num_players, u8 max_spectators, u32 input_size);

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

	private:
		bool _started;

		u32 _input_size;

		u8 _num_players;
		u8 _max_spectators;

		SyncSystem _sync;

		MessageSystem _msg;

		NetAdapter* _host;
	};
}