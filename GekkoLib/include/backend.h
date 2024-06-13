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

    struct ChecksumEntry {
        Frame frame = -1;
        u32 checksum {};
    };

	class Player
	{
	public:
		Player(Handle phandle, PlayerType type, NetAddress* addr, u32 magic = 0);

		PlayerType GetType();

		PlayerStatus GetStatus();

		void SetStatus(PlayerStatus type);

        void SetChecksum(Frame frame, u32 checksum);

	public:
		Handle handle;

		u8 sync_num;

		u16 session_magic;

		NetStats stats;

		NetAddress address;

        static const i32 NUM_CHECKSUMS = 16;

        ChecksumEntry health[NUM_CHECKSUMS];

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
		static const i32 HISTORY_SIZE = 26;

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

		void HandleData(std::vector<std::unique_ptr<NetResult>>& data);

        std::queue<std::unique_ptr<NetInputData>>& LastReceivedInputs();

		void SendInputAck(Handle player, Frame frame);

		Frame GetLastAddedInput(bool spectator = false);

		bool CheckStatusActors();

        void SendHealthCheck(Frame frame, u32 checksum);

	public:
		std::vector<std::unique_ptr<Player>> locals;

		std::vector<std::unique_ptr<Player>> remotes;

		std::vector<std::unique_ptr<Player>> spectators;

		AdvantageHistory history;

        SessionEventSystem session_events;

        ChecksumEntry local_health[Player::NUM_CHECKSUMS];

	private:
		void SendSyncRequest(NetAddress* addr);

		void SendSyncResponse(NetAddress* addr, u16 magic);

		void AddPendingInput(bool spectator = false);

		std::vector<Handle> GetHandlesForAddress(NetAddress* addr);

		Player* GetPlayerByHandle(Handle handle);

		Frame GetMinLastAckedFrame(bool spectator = false);

		void HandleTooFarBehindActors(bool spectator = false);

		u64 TimeSinceEpoch();

        void SendDataToAll(NetData* pkt, NetAdapter* host, bool spectators_only = false);

        void SendDataTo(NetData* pkt, NetAdapter* host);

        void ParsePacket(NetAddress& addr, NetPacket& pkt);

        void OnSyncRequest(NetAddress& addr, NetPacket& pkt);

        void OnSyncResponse(NetAddress& addr, NetPacket& pkt);

        void OnInputs(NetAddress& addr, NetPacket& pkt);

        void OnInputAck(NetAddress& addr, NetPacket& pkt);

        void OnHealthCheck(NetAddress& addr, NetPacket& pkt);

	private:
		static const u32 MAX_PLAYER_SEND_SIZE = 32;
		static const u32 MAX_SPECTATOR_SEND_SIZE = 48;
		static const u32 NUM_TO_SYNC = 4;

		u32 _input_size;

		u16 _session_magic;

		Frame _last_added_input;

		Frame _last_added_spectator_input;

		std::list<u8*> _player_input_send_list;

		std::list<u8*> _spectator_input_send_list;

		std::queue<std::unique_ptr<NetData>> _pending_output;

		std::queue<std::unique_ptr<NetInputData>> _received_inputs;

        std::vector<u8> _bin_buffer;
	};
}
