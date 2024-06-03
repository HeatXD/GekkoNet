#pragma once

#include "gekko_types.h"
#include "net.h"
#include "event.h"

#include <memory>
#include <list>
#include <vector>
#include <queue>
#include <chrono>

namespace Gekko {

	enum PlayerType {
		LocalPlayer,
		RemotePlayer,
		Spectator
	};

	enum PlayerStatus {
		Initiating,
		Connected,
		Disconnected,
	};

	class Player
	{
	public:
		Player(Handle phandle, PlayerType type, NetAddress* addr, u32 magic = 0);

		PlayerType GetType();

		PlayerStatus GetStatus();

		void SetStatus(PlayerStatus type);

	public:
		Handle handle;

		u8 sync_num;

		u32 session_magic;

		NetStats stats;

		NetAddress address;

	private:
		PlayerType _type;

		PlayerStatus _status;
	};

	struct AdvantageHistory {
	public:
		void Init();

		void Update(Frame frame);

		f32 GetAverageAdvantage();

		i8 GetLocalAdvantage();

		void SetLocalAdvantage(i8 adv);

		void AddRemoteAdvantage(i8 adv);

	private:
		static const i32 HISTORY_SIZE = 32;

        u8 _adv_index;

        i8 _local_frame_adv;

		i8 _local[HISTORY_SIZE];

		i8 _remote[HISTORY_SIZE];

        i8 _remote_frame_adv[HISTORY_SIZE];
	};

	class MessageSystem {
	public:
		MessageSystem();

		void Init(u32 input_size);

		void AddInput(Frame input_frame, u8 input[]);

		void AddSpectatorInput(Frame input_frame, u8 input[]);

		void SendPendingOutput(NetAdapter* host);

		void HandleData(std::vector<NetData*>& data, bool session_started);

		u32 GetMagic();

		std::queue<NetInputData*>& LastReceivedInputs();

		void SendInputAck(Handle player, Frame frame);

		Frame GetLastAddedInput(bool spectator = false);

		bool CheckStatusActors();

	public:
		std::vector<std::unique_ptr<Player>> locals;

		std::vector<std::unique_ptr<Player>> remotes;

		std::vector<std::unique_ptr<Player>> spectators;

		AdvantageHistory history;

	private:
		void SendSyncRequest(NetAddress* addr);

		void SendSyncResponse(NetAddress* addr, u32 magic);

		void AddPendingInput(bool spectator = false);

		std::vector<Handle> GetHandlesForAddress(NetAddress* addr);

		Player* GetPlayerByHandle(Handle handle);

		Frame GetMinLastAckedFrame(bool spectator = false);

		void HandleTooFarBehindActors(bool spectator = false);

		u64 TimeSinceEpoch();

	private:
		static const u32 MAX_PLAYER_SEND_SIZE = 32;
		static const u32 MAX_SPECTATOR_SEND_SIZE = 48;
		static const u32 NUM_TO_SYNC = 4;

		u32 _input_size;

		u32 _session_magic;

		Frame _last_added_input;

		Frame _last_added_spectator_input;

		std::list<u8*> _player_input_send_list;

		std::list<u8*> _spectator_input_send_list;

		std::queue<NetData*> _pending_output;

		std::queue<NetInputData*> _received_inputs;

        std::shared_ptr<SessionEventSystem> _session_events;
	};
}
