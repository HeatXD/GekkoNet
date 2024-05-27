#include "backend.h"
#include "input.h"
#include <chrono>

Gekko::u32 Gekko::NetAddress::GetSize()
{
	return _size;
}

Gekko::NetAddress::NetAddress(void* data, u32 size)
{
	_size = size;
	_data = std::unique_ptr<u8[]>(new u8[_size]);
	// copy address data
	std::memcpy(_data.get(), data, _size);
}

Gekko::NetAddress::NetAddress()
{
	_size = 0;
	_data = nullptr;
}

void Gekko::NetAddress::Copy(NetAddress* other)
{
	if (!other)
		return;

	_size = other->_size;

	if (_data) 
		_data.reset();

	_data = std::unique_ptr<u8[]>(new u8[_size]);
	// copy address data
	std::memcpy(_data.get(), other->GetAddress(), _size);
}

bool Gekko::NetAddress::Equals(NetAddress& other)
{
	return _size == other._size && std::memcmp(_data.get(), other._data.get(), _size) == 0;
}

Gekko::u8* Gekko::NetAddress::GetAddress()
{
	return _data.get();
}

Gekko::MessageSystem::MessageSystem()
{
	_input_size = 0;
	_last_added_input = GameInput::NULL_FRAME;
	_last_added_spectator_input = GameInput::NULL_FRAME;

	// gen magic for session
	std::srand(std::time(nullptr));
	_session_magic = std::rand();

	history = AdvantageHistory();
}

void Gekko::MessageSystem::Init(u32 input_size)
{
	_input_size = input_size;
	_last_added_input = GameInput::NULL_FRAME;
	_last_added_spectator_input = GameInput::NULL_FRAME;

	history.Init();
}


void Gekko::MessageSystem::AddInput(Frame input_frame, u8 input[])
{
	if (_last_added_input + 1 == input_frame) {
		_last_added_input++;
		_player_input_send_list.push_back(new u8[_input_size * locals.size()]);
		std::memcpy(_player_input_send_list.back(), input, _input_size * locals.size());

		// update history
		history.Update(input_frame);
	}

	const Frame min_ack = GetMinLastAckedFrame(false);
	const u32 diff = _last_added_input - min_ack;

	if (diff > MAX_PLAYER_SEND_SIZE && min_ack != INT_MAX) {
		// disconnect the offending players
		HandleTooFarBehindActors(false);
	}

	if (_player_input_send_list.size() > std::min(MAX_PLAYER_SEND_SIZE, diff)) {
		delete _player_input_send_list.front();
		_player_input_send_list.pop_front();
	}
}

void Gekko::MessageSystem::AddSpectatorInput(Frame input_frame, u8 input[])
{
	if (_last_added_spectator_input + 1 == input_frame) {
		_last_added_spectator_input++;
		_spectator_input_send_list.push_back(new u8[_input_size * (locals.size() + remotes.size())]);
		std::memcpy(_spectator_input_send_list.back(), input, _input_size * (locals.size() + remotes.size()));
	}

	const Frame min_ack = GetMinLastAckedFrame(true);
	const u32 diff = _last_added_spectator_input - min_ack;

	if (diff > MAX_SPECTATOR_SEND_SIZE && min_ack != INT_MAX) {
		// disconnect the offending spectators
		HandleTooFarBehindActors(true);
	}

	if (_spectator_input_send_list.size() > std::min(MAX_SPECTATOR_SEND_SIZE, diff)) {
		delete _spectator_input_send_list.front();
		_spectator_input_send_list.pop_front();
	}
}

void Gekko::MessageSystem::SendPendingOutput(NetAdapter* host)
{
	if (!host) return;

	// add input packet
	if (!_player_input_send_list.empty() && !remotes.empty()) {
		AddPendingInput(false);
	}

	// add spectator input packet
	if (!_spectator_input_send_list.empty() && !spectators.empty()) {
		AddPendingInput(true);
	}

	// handle messages
	for (u32 i = 0; i < _pending_output.size(); i++) {
		auto pkt = _pending_output.front();
		if (pkt->pkt.type == Inputs) {
			for (u32 i = 0; i < remotes.size(); i++) {
				if (remotes[i]->address.GetSize() != 0) {
					// copy addr, set magic and send it off!
					pkt->addr.Copy(&remotes[i]->address);
					pkt->pkt.magic = remotes[i]->session_magic;
					host->SendData(pkt->addr, pkt->pkt);
				}
			}
			// now when done we can cleanup the inputs since we used malloc
			std::free(pkt->pkt.x.input.inputs);
		}
		else if (pkt->pkt.type == SpectatorInputs) {
			for (u32 i = 0; i < spectators.size(); i++) {
				if (spectators[i]->address.GetSize() != 0) {
					// copy addr, set magic and send it off!
					pkt->addr.Copy(&spectators[i]->address);
					pkt->pkt.magic = spectators[i]->session_magic;
					host->SendData(pkt->addr, pkt->pkt);
				}
			}
			// now when done we can cleanup the inputs since we used malloc
			std::free(pkt->pkt.x.input.inputs);
		}
		else {
			host->SendData(pkt->addr, pkt->pkt);
		}
		// cleanup packet
		delete pkt;
		// housekeeping
		_pending_output.pop();
	}
}

void Gekko::MessageSystem::HandleData(std::vector<NetData*>& data, bool session_started)
{
	u64 now = TimeSinceEpoch();

	for (u32 i = 0; i < data.size(); i++) {
		// handle connection events
		auto type = data[i]->pkt.type;
		if (type == SyncRequest) {
			i32 should_send = 0;
			// handle and set the peer its session magic
			// but when the session has started only allow spectators
			if (!session_started) {
				for (u32 j = 0; j < remotes.size(); j++) {
					if (remotes[j]->address.Equals(data[i]->addr)) {
						remotes[j]->session_magic = data[i]->pkt.x.sync_request.rng_data;
						if (remotes[j]->sync_num == 0) {
							remotes[j]->stats.last_sent_sync_message = now;
							should_send++;
						}
					}
				}
			}

			for (u32 j = 0; j < spectators.size(); j++) {
				if (spectators[j]->address.Equals(data[i]->addr)) {
					spectators[j]->session_magic = data[i]->pkt.x.sync_request.rng_data;
					if (spectators[j]->sync_num == 0) {
						spectators[j]->stats.last_sent_sync_message = now;
						should_send++;
					}
				}
			}

			if (should_send > 0) {
				// send a packet containing the local session magic
				SendSyncResponse(&data[i]->addr, data[i]->pkt.x.sync_response.rng_data);
			}
			// cleanup packet
			delete data[i];
			continue;
		}

		if (data[i]->pkt.magic != _session_magic) {
			// cleanup packet
			delete data[i];
			continue;
		}

		// handle other events
		if (type == SyncResponse) {
			i32 stop_sending = 0; 

			for (u32 j = 0; j < remotes.size(); j++) {
				if (remotes[j]->GetStatus() == Connected) continue;

				if (remotes[j]->address.Equals(data[i]->addr)) {
					remotes[j]->session_magic = data[i]->pkt.x.sync_request.rng_data;

					if (remotes[j]->sync_num < NUM_TO_SYNC) {
						remotes[j]->sync_num++;
						stop_sending--;
						printf("handle:%d syncing:(%d/%d)\n", remotes[j]->handle, remotes[j]->sync_num, NUM_TO_SYNC);
						continue;
					}

					if (remotes[j]->sync_num == NUM_TO_SYNC) {
						remotes[j]->SetStatus(Connected);
						stop_sending++;
						printf("handle:%d connected!\n", remotes[j]->handle);
						continue;
					}
				}
			}

			for (u32 j = 0; j < spectators.size(); j++) {
				if (spectators[j]->GetStatus() == Connected) continue;

				if (spectators[j]->address.Equals(data[i]->addr)) {
					spectators[j]->session_magic = data[i]->pkt.x.sync_request.rng_data;

					if (spectators[j]->sync_num < NUM_TO_SYNC) {
						spectators[j]->sync_num++;
						stop_sending--;
						printf("handle:%d syncing:(%d/%d)\n", spectators[j]->handle, spectators[j]->sync_num, NUM_TO_SYNC);
						continue;
					}

					if (spectators[j]->sync_num == NUM_TO_SYNC) {
						spectators[j]->SetStatus(Connected);
						stop_sending++;
						printf("handle:%d connected!\n", spectators[j]->handle);
						// TODO SEND CONNECT INIT MESSAGE WITH GAMESTATE IF NEEDED
						continue;
					}
				}
			}

			if (stop_sending < 0) {
				// send a packet containing the local session magic
				SendSyncResponse(&data[i]->addr, data[i]->pkt.x.sync_response.rng_data);
			}

			delete data[i];
			continue;
		}

		if (type == Inputs || type == SpectatorInputs) {
			// printf("recv inputs from netaddr:%d\n", *data[i]->addr.GetAddress());

			auto net_input = new NetInputData;

			net_input->handles = GetHandlesForAddress(&data[i]->addr);

			net_input->input.total_size = data[i]->pkt.x.input.total_size;
			net_input->input.input_count = data[i]->pkt.x.input.input_count;
			net_input->input.start_frame = data[i]->pkt.x.input.start_frame;

			net_input->input.inputs = (u8*)std::malloc(net_input->input.total_size);

			if (net_input->input.inputs)
				std::memcpy(net_input->input.inputs, data[i]->pkt.x.input.inputs, net_input->input.total_size);

			// now when done we can cleanup the inputs since we used malloc
			std::free(data[i]->pkt.x.input.inputs);

			_received_inputs.push(net_input);

			delete data[i];
			continue;
		}

		if(type == InputAck) {
			// we should just update the ack frame for all handles where the address matches
			const Frame ack_frame = data[i]->pkt.x.input_ack.ack_frame;

			for (auto player : remotes) {
				if (player->address.Equals(data[i]->addr)) {
					if (player->stats.last_acked_frame < ack_frame) {
						player->stats.last_acked_frame = ack_frame;
					}
				}
			}

			for (auto player : spectators) {
				if (player->address.Equals(data[i]->addr)) {
					if (player->stats.last_acked_frame < ack_frame) {
						player->stats.last_acked_frame = ack_frame;
					}
				}
			}

			const i32 remote_advantage = data[i]->pkt.x.input_ack.frame_advantage;
			history.AddRemoteAdvantage(remote_advantage);

			delete data[i];
			continue;
		}
	}
}

void Gekko::MessageSystem::SendSyncRequest(NetAddress* addr)
{
	if (!addr) return;

	auto message = new NetData;

	message->addr.Copy(addr);
	message->pkt.type = SyncRequest;
	message->pkt.magic = 0;
	message->pkt.x.sync_request.rng_data = _session_magic;

	_pending_output.push(message);
}

void Gekko::MessageSystem::SendSyncResponse(NetAddress* addr, u32 magic)
{
	if (!addr || magic == 0) return;

	auto message = new NetData;

	message->addr.Copy(addr);
	message->pkt.type = SyncResponse;
	message->pkt.magic = magic;
	message->pkt.x.sync_request.rng_data = _session_magic;

	_pending_output.push(message);
}

Gekko::u32 Gekko::MessageSystem::GetMagic()
{
	return _session_magic;
}

std::queue<Gekko::NetInputData*>& Gekko::MessageSystem::LastReceivedInputs()
{
	return _received_inputs;
}

void Gekko::MessageSystem::SendInputAck(Handle player, Frame frame)
{
	auto plyr = GetPlayerByHandle(player);

	if (!plyr) return;

	auto message = new NetData;

	message->addr.Copy(&plyr->address);
	message->pkt.magic = plyr->session_magic;

	message->pkt.type = InputAck;
	message->pkt.x.input_ack.ack_frame = frame;
	message->pkt.x.input_ack.frame_advantage = history.GetLocalAdvantage();
	
	_pending_output.push(message);
}

std::vector<Gekko::Handle> Gekko::MessageSystem::GetHandlesForAddress(NetAddress* addr)
{
	auto result = std::vector<Handle>();
	for (auto player: remotes) {
		if (player->address.Equals(*addr)) {
			result.push_back(player->handle);
		}
	}
	return result;
}

Gekko::Player* Gekko::MessageSystem::GetPlayerByHandle(Handle handle) 
{
	for (auto player: remotes) {
		if (player->handle == handle) {
			return player;
		}
	}
	return nullptr;
}

Gekko::Frame Gekko::MessageSystem::GetMinLastAckedFrame(bool spectator) 
{
	Frame min = INT_MAX;
	for (auto player : spectator ? spectators : remotes) {
		if (player->GetStatus() == Connected) {
			min = std::min(player->stats.last_acked_frame, min);
		}
	}
	return min;
}

Gekko::Frame Gekko::MessageSystem::GetLastAddedInput(bool spectator) {
	return spectator ? _last_added_spectator_input : _last_added_input;
}

bool Gekko::MessageSystem::CheckStatusActors()
{
	i32 result = 0;
	u64 now = TimeSinceEpoch();

	for (Player* player : remotes) {
		if (player->GetStatus() == Initiating) {
			if (player->stats.last_sent_sync_message + NetStats::SYNC_MSG_DELAY < now) {
				if (player->sync_num == 0) {
					SendSyncRequest(&player->address);
					player->stats.last_sent_sync_message = now;
				}
				else if (player->sync_num < NUM_TO_SYNC) {
					SendSyncResponse(&player->address, player->session_magic);
					player->stats.last_sent_sync_message = now;
				}
			}
			result--;
		}
	}

	for (Player* player : spectators) {
		if (player->GetStatus() == Initiating) {
			if (player->stats.last_sent_sync_message + NetStats::SYNC_MSG_DELAY < now) {
				if (player->sync_num == 0) {
					SendSyncRequest(&player->address);
					player->stats.last_sent_sync_message = now;
				}
				else if (player->sync_num < NUM_TO_SYNC) {
					SendSyncResponse(&player->address, player->session_magic);
					player->stats.last_sent_sync_message = now;
				}
			}
			result--;
		}
	}

	return result == 0;
}

void Gekko::MessageSystem::HandleTooFarBehindActors(bool spectator)
{
	const u32 max_diff = spectator ? MAX_SPECTATOR_SEND_SIZE : MAX_PLAYER_SEND_SIZE;
	const Frame last_added = spectator ? _last_added_spectator_input : _last_added_input;

	for (auto player : spectator ? spectators : remotes) {
		if (player->GetStatus() == Connected) {
			const u32 diff = last_added - player->stats.last_acked_frame;
			if (diff > max_diff) {
				player->SetStatus(Disconnected);
				player->sync_num = 0;
				printf("handle:%d  disconnected!\n", player->handle);
			}
		}
	}
}

Gekko::u64 Gekko::MessageSystem::TimeSinceEpoch()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void Gekko::MessageSystem::AddPendingInput(bool spectator)
{
	const auto& send_list = spectator ? _spectator_input_send_list : _player_input_send_list;

	const Frame last_added = spectator ? _last_added_spectator_input : _last_added_input;

	const u32 num_players = spectator ? (u32)(locals.size() + remotes.size()) : (u32)locals.size();
	const u32 input_size = _input_size * num_players;
	const u32 input_count = (u32)send_list.size();
	const u32 total_size = input_size * input_count;

	std::unique_ptr<u8[]> inputs(new u8[total_size]);

	u32 idx = 0;
	for (auto& input : send_list) {
		std::memcpy(&inputs[idx * input_size], input, input_size);
		idx++;
	}

	// TODO? maybe add some type of compression to the inputs.
	// the address and magic is set later so dont worry about it now

	auto data = new NetData;

	data->pkt.type = spectator ? SpectatorInputs : Inputs;
	data->pkt.x.input.input_count = (u32)send_list.size();
	data->pkt.x.input.start_frame = last_added - (u32)send_list.size();
	data->pkt.x.input.inputs = (u8*)std::malloc(total_size);
	data->pkt.x.input.total_size = total_size;
	
	if(data->pkt.x.input.inputs)
		std::memcpy(data->pkt.x.input.inputs, inputs.get(), total_size);

	_pending_output.push(data);
}

void Gekko::AdvantageHistory::Init()
{
	_local_frame_adv = 0;
	_remote_frame_adv.clear();

	std::memset(_local, 0, HISTORY_SIZE * sizeof(i32));
	std::memset(_remote, 0, HISTORY_SIZE * sizeof(i32));
}

void Gekko::AdvantageHistory::Update(Frame frame)
{
	const u32 update_frame = std::max(frame, 0);

	_local[update_frame % HISTORY_SIZE] = _local_frame_adv;

	if (!_remote_frame_adv.empty()) {
		i32 max = INT_MIN;
		for (i32 num : _remote_frame_adv) {
			max = std::max(max, num);
		}
		_remote[update_frame % HISTORY_SIZE] = max == INT_MIN ? 0 : max;
	}
	else {
		_remote[update_frame % HISTORY_SIZE] = 0;
	}
}

Gekko::f32 Gekko::AdvantageHistory::GetAverageAdvantage()
{
	f32 sum_local = 0.f;
	f32 sum_remote = 0.f;

	for (i32 i = 0; i < HISTORY_SIZE; i++) {
		sum_local += _local[i];
		sum_remote += _remote[i];
	}

	f32 avg_local = sum_local / HISTORY_SIZE;
	f32 avg_remote = sum_remote / HISTORY_SIZE;

	// return the average frames ahead
	return avg_local - avg_remote;
}

void Gekko::AdvantageHistory::SetLocalAdvantage(i32 adv) {
	_local_frame_adv = adv;
}

void Gekko::AdvantageHistory::AddRemoteAdvantage(i32 adv) {
	_remote_frame_adv.push_front(adv);
	// clean up
	if (_remote_frame_adv.size() > 48) {
		for (i32 i = 0; i < 12; i++) {
			_remote_frame_adv.pop_back();
		}
	}
}

Gekko::i32 Gekko::AdvantageHistory::GetLocalAdvantage() {
	return _local_frame_adv;
}