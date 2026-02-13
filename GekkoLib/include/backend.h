#pragma once

#include "gekkonet.h"

#include "gekko_types.h"

#include "compression.h"
#include "net.h"
#include "event.h"

#include <memory>
#include <list>
#include <vector>
#include <queue>
#include <chrono>
#include <map>

namespace Gekko {

    enum PlayerStatus {
        Initiating,
        Connected,
        Disconnected,
    };

	struct InputCache {
		Frame last_acked_frame = -1;
		Frame last_input_frame = -1;
		std::vector<InputMsg> packets;

		bool IsValid(Frame current_ack, Frame current_last_input) const;
	};

	class Player
	{
	public:
		Player(Handle phandle, GekkoPlayerType type, NetAddress* addr, u32 magic = 0);

        GekkoPlayerType GetType();

		PlayerStatus GetStatus();

		void SetStatus(PlayerStatus type);

        void SetChecksum(Frame frame, u32 checksum);

	public:
		Handle handle;

		u8 sync_num;

		u16 session_magic;

		NetStats stats;

		NetAddress address;

        std::map<Frame, u32> session_health;

		InputCache input_cache;

	private:
        GekkoPlayerType _type;

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

        void Init(u8 num_players, u32 input_size);

		void AddInput(Frame input_frame, Handle player, u8 input[], bool remote = false);

		void AddSpectatorInput(Frame input_frame, u8 input[]);

		void SendPendingOutput(GekkoNetAdapter* host);

		void HandleData(GekkoNetAdapter* host, GekkoNetResult** data, u32 length);

		void SendInputAck(Handle player, Frame frame);

		Frame GetLastAddedInput(bool spectator = false);

		bool CheckStatusActors();

        void SendSessionHealth(Frame frame, u32 checksum);

        void SendNetworkHealth();

        Frame GetLastAddedInputFrom(Handle player);

        std::deque<std::unique_ptr<u8[]>>& GetNetPlayerQueue(Handle player);

	public:
		std::vector<std::unique_ptr<Player>> locals;

		std::vector<std::unique_ptr<Player>> remotes;

		std::vector<std::unique_ptr<Player>> spectators;

		AdvantageHistory history;

        SessionEventSystem session_events;

        std::map<Frame, u32> local_health;

        struct NetInputQueue {
            Frame last_added_input = -1;
            std::deque<std::unique_ptr<u8[]>> inputs;

            NetInputQueue(const NetInputQueue&) = delete;
            NetInputQueue& operator=(const NetInputQueue&) = delete;

            NetInputQueue(NetInputQueue&&) = default;
            NetInputQueue& operator=(NetInputQueue&&) = default;

            NetInputQueue() = default;

            void TrimToAck(Frame min_ack, u32 max_size);
        };

	private:
		void SendSyncRequest(NetAddress* addr);

		void SendSyncResponse(NetAddress* addr, u16 magic);

		void SendInputsToPeer(Player* peer, GekkoNetAdapter* host, bool spectator);

		std::vector<Handle> GetRemoteHandlesForAddress(NetAddress* addr);

		Player* GetPlayerByHandle(Handle handle);

		Frame GetMinLastAckedFrame(bool spectator = false);

		void HandleTooFarBehindActors(bool spectator = false);

		u64 TimeSinceEpoch();

        void SendDataToAll(NetData* pkt, GekkoNetAdapter* host, bool spectators_only = false);

        void SendDataTo(NetData* pkt, GekkoNetAdapter* host);

        void ParsePacket(NetAddress& addr, NetPacket& pkt, u32 packet_size);

        void OnSyncRequest(NetAddress& addr, NetPacket& pkt);

        void OnSyncResponse(NetAddress& addr, NetPacket& pkt);

        void OnInputs(NetAddress& addr, NetPacket& pkt);

        void OnInputAck(NetAddress& addr, NetPacket& pkt);

        void OnSessionHealth(NetAddress& addr, NetPacket& pkt);

        void OnNetworkHealth(NetAddress& addr, NetPacket& pkt);

	private:
		const u32 MAX_INPUT_QUEUE_SIZE = 128;
	    const u32 NUM_TO_SYNC = 4;

		u32 _input_size;

		u16 _session_magic;

        u8  _num_players;

        // input queue for each player for either sending or receiving
        std::vector<NetInputQueue> _net_player_queue;

        // input queue for spectator inputs
        NetInputQueue _net_spectator_queue;

		std::queue<std::unique_ptr<NetData>> _pending_output;

        std::vector<u8> _bin_buffer;

        u64 _last_sent_network_check;
	};
}
