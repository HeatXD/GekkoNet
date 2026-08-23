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

	struct AdvantageHistory {
	public:
		void Init();

		void Update(Frame frame);

		f32 GetAverageAdvantage();

		void SetLocalAdvantage(i8 adv);

		void SetRemoteAdvantage(i8 adv);

	private:
		static const i32 HISTORY_SIZE = 26;

        i8 _local_frame_adv;

        i8 _remote_frame_adv;

		i8 _local[HISTORY_SIZE];

		i8 _remote[HISTORY_SIZE];
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

		u64 last_input_send_time = 0;

		u8 disconnect_msgs_left = 0;

		u64 last_disconnect_msg_time = 0;

        // Late spectators load this state before the host starts sending inputs.
        bool requires_spectator_state = false;

        bool spectator_state_configured = false;

        bool spectator_state_acked = true;

        Frame spectator_state_frame = GameInput::NULL_FRAME;

        std::vector<u8> spectator_state;

        u64 last_spectator_state_send_time = 0;

		// the disconnect frames this peer last claimed per disconnected player.
		std::map<Handle, Frame> peer_claims;

		// the disconnect frames we last claimed towards this peer.
		std::map<Handle, Frame> peer_claims_sent;

		// the session wide agreed frame after which this players inputs are voided.
		Frame disconnect_frame = INT32_MAX;

		// the disconnect frame the inputs fed to the session are currently based on.
		Frame applied_disconnect_frame = INT32_MAX;

		// timestamps driving the claim exchange for this disconnected player.
		u64 last_claim_sent_time = 0;

		u64 last_claim_raise_time = 0;

		AdvantageHistory adv_history;

	private:
        GekkoPlayerType _type;

		PlayerStatus _status;
	};

	class MessageSystem {
	public:
		MessageSystem();

		void Init(
            u8 num_players,
            u32 input_size,
            u32 state_size = 0,
            bool accept_spectator_state = false
        );

		void AddInput(Frame input_frame, Handle player, u8 input[], bool remote = false);

		void AddSpectatorInput(Frame input_frame, u8 input[]);

		void SendPendingOutput(GekkoNetAdapter* host);

		void HandleData(GekkoNetAdapter* host, GekkoNetResult** data, u32 length);

		void SendInputAck(Handle player, Frame frame, i8 local_advantage);

		Frame GetLastAddedInput(bool spectator = false);

		bool CheckStatusActors();

        bool DisconnectActor(Handle handle);

        void ReclaimDisconnectedSpectators();

		void SetDisconnectTimeout(u32 timeout);

		Frame GetDisconnectHoldFrame();

        void SendSessionHealth(Frame frame, u32 checksum);

        void SendNetworkHealth();

        Frame GetLastAddedInputFrom(Handle player);

        std::deque<std::unique_ptr<u8[]>>& GetNetPlayerQueue(Handle player);

        Frame GetOldestSpectatorInput();

        void SetSpectatorState(Handle spectator, Frame frame, const u8* state, u32 state_size);

        bool TakeSpectatorState(Frame& frame, std::vector<u8>& state);

	public:
		std::vector<std::unique_ptr<Player>> locals;

		std::vector<std::unique_ptr<Player>> remotes;

		std::vector<std::unique_ptr<Player>> spectators;

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

		void SendDisconnect(NetAddress* addr, u16 magic);

		void SendPendingDisconnects();

		void SendInputsToPeer(Player* peer, GekkoNetAdapter* host, bool spectator);

        void SendPendingSpectatorStates(GekkoNetAdapter* host);

		std::vector<Handle> GetRemoteHandlesForAddress(NetAddress* addr);

		Player* GetPlayerByHandle(Handle handle);

		Frame GetMinLastAckedFrame(bool spectator = false);

		void HandleTooFarBehindActors(bool spectator = false);

		void MarkActorDisconnected(Player* actor);

		void SendPendingClaims();

		void HandleUnrecoverableGap();

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

        void OnSpectatorState(NetAddress& addr, NetPacket& pkt);

        void OnSpectatorStateAck(NetAddress& addr, NetPacket& pkt);

        void OnDisconnect(NetAddress& addr, NetPacket& pkt);

        void OnDisconnectClaim(NetAddress& addr, NetPacket& pkt);

	private:
		const u32 MAX_INPUT_QUEUE_SIZE = 128;
	    const u32 NUM_TO_SYNC = 4;
		const u8 NUM_DISCONNECT_MSGS = 5;

        const u32 SPECTATOR_STATE_CHUNK_SIZE = 900;

		u32 _input_size;

        u32 _state_size;

        bool _accept_spectator_state;

		u16 _session_magic;

		u64 _disconnect_timeout;

        u8  _num_players;

        // input queue for each player for either sending or receiving
        std::vector<NetInputQueue> _net_player_queue;

        // input queue for spectator inputs
        NetInputQueue _net_spectator_queue;

        struct IncomingSpectatorState {
            bool active = false;
            bool ready = false;
            bool taken = false;
            Frame frame = GameInput::NULL_FRAME;
            u32 total_size = 0;
            u32 received_chunks = 0;
            std::vector<u8> state;
            std::vector<bool> chunks;
        } _incoming_spectator_state;

        struct PendingDisconnect {
            NetAddress address;
            u16 session_magic = 0;
            u8 messages_left = 0;
            u64 last_message_time = 0;
        };

        std::vector<std::unique_ptr<PendingDisconnect>> _pending_disconnects;

		std::queue<std::unique_ptr<NetData>> _pending_output;

        std::vector<u8> _bin_buffer;

        u64 _last_sent_network_check;
	};
}
