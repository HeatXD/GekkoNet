#pragma once

#include <vector>

#include "gekko_types.h"
#include "player.h"
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

		void UpdatePlayerStatus();

		bool AllPlayersValid();

	private:
		bool _started;

		u32 _input_size;

		u8 _num_players;
		u8 _max_spectators;

		std::vector<Player> _players;
		std::vector<Player> _spectators;

		SyncSystem _sync;

		NetAdapter* _host;
	};
}