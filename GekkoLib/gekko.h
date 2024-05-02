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

		Handle AddPlayer(PlayerType type, NetAddress addr = NetAddress());

		void AddLocalInput(Handle player, Input input);

		std::vector<Event> UpdateSession();

	private:
		void Poll();

		bool AllPlayersValid();

	private:
		bool _started;

		u32 _input_size;

		u64 _session_magic;

		u8 _num_players;
		u8 _max_spectators;

		SyncSystem _sync;

		MessageSystem _msg;

		NetAdapter* _host;
	};
}