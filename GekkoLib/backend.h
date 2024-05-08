#pragma once

#include "gekko_types.h"

#include <memory>
#include <list>
#include <vector>
#include <queue>

namespace Gekko {

	struct NetAddress {
		NetAddress();
		NetAddress(void* data, u32 size);

		u8* GetAddress();
		u32 GetSize();

		void Copy(NetAddress* other);
		bool Equals(NetAddress& other);

	private:
		std::unique_ptr<u8[]> _data;
		u32 _size;
	};

	enum PacketType {
		Inputs = 1,
		SpectatorInputs,
		InputAck,
		SyncRequest,
		SyncResponse,
	};

	struct NetPacket {
		PacketType type;
		u32 magic;

		union data {
			struct Input {
				u32 total_size;
				Frame start_frame;
				u32 input_count;
				u8* inputs;
			} input;
			struct SyncRequest {
				u32 rng_data;
			} sync_request;
			struct SyncResponse {
				u32 rng_data;
			} sync_response;
		}x;
	};

	struct NetData {
		NetAddress addr;
		NetPacket pkt;
	};

	struct NetStats {
		
	};

	struct NetInputData {
		std::vector<Handle> handles;
		NetPacket::data::Input input;
	};

	class NetAdapter {
	public:
		virtual std::vector<NetData*> ReceiveData() = 0;
		virtual void SendData(NetAddress& addr, NetPacket& pkt) = 0;
	};

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
		u32 session_magic;
		NetAddress address;
		u8 sync_num;

	private:
		PlayerType _type;
		PlayerStatus _status;
		NetStats _stats;
	};

	class MessageSystem {
	public:
		MessageSystem();

		void Init(u32 input_size);

		void AddInput(Frame input_frame, u8 input[]);

		void AddSpectatorInput(Frame input_frame, u8 input[]);

		void SendPendingOutput(NetAdapter* host);

		void HandleData(std::vector<NetData*>& data, bool session_started);

		void SendSyncRequest(NetAddress* addr);

		u32 GetMagic();

		std::queue<NetInputData*>& LastReceivedInputs();

	public:
		std::vector<Player*> locals;
		std::vector<Player*> remotes;
		std::vector<Player*> spectators;

	private:
		void AddPendingInput(bool spectator = false);

		std::vector<Handle> GetHandlesForAddress(NetAddress* addr);

	private:
		static const u32 MAX_PLAYER_SEND_SIZE = 16;
		static const u32 MAX_SPECTATOR_SEND_SIZE = 32;
		static const u32 NUM_TO_SYNC = 4;

		u32 _input_size;
		u32 _session_magic;

		Frame _last_added_input;
		Frame _last_added_spectator_input;

		std::list<u8*> _player_input_send_list;
		std::list<u8*> _spectator_input_send_list;

		std::queue<NetData*> _pending_output;

		std::queue<NetInputData*> _received_inputs;
	};
}