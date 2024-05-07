#include "backend.h"
#include "input.h"

Gekko::u32 Gekko::NetAddress::GetSize()
{
	return _size;
}

Gekko::NetAddress::NetAddress(u8* data, u32 size)
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
}

void Gekko::MessageSystem::Init(u32 input_size)
{
	_input_size = input_size;
	_last_added_input = GameInput::NULL_FRAME;
	_last_added_spectator_input = GameInput::NULL_FRAME;
}


void Gekko::MessageSystem::AddInput(Frame input_frame, u8 input[])
{
	if (_last_added_input + 1 == input_frame) {
		_last_added_input++;
		_player_input_send_list.push_back(new u8[_input_size * locals.size()]);
		std::memcpy(_player_input_send_list.back(), input, _input_size * locals.size());
	}

	if (_player_input_send_list.size() > MAX_PLAYER_SEND_SIZE) {
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

	if (_spectator_input_send_list.size() > MAX_SPECTATOR_SEND_SIZE) {
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
		if (pkt->pkt.type == PacketType::Inputs) {
			for (size_t i = 0; i < remotes.size(); i++) {
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
		else if (pkt->pkt.type == PacketType::SpectatorInputs) {
			for (size_t i = 0; i < spectators.size(); i++) {
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
	for (u32 i = 0; i < data.size(); i++) {
		// handle connection events
		auto type = data[i]->pkt.type;
		if (type == PacketType::SyncRequest) {
			auto message = new NetData;
			i32 should_send = 0;
			// handle and set the peer its session magic
			// but when the session has started only allow spectators
			if (!session_started) {
				for (u32 j = 0; j < remotes.size(); j++) {
					if (remotes[j]->address.Equals(data[i]->addr)) {
						remotes[j]->session_magic = data[i]->pkt.x.sync_request.rng_data;
						if (remotes[j]->sync_num == 0) should_send++; else should_send--;
					}
				}
			}

			for (u32 j = 0; j < spectators.size(); j++) {
				if (spectators[j]->address.Equals(data[i]->addr)) {
					spectators[j]->session_magic = data[i]->pkt.x.sync_request.rng_data;
					if (spectators[j]->sync_num == 0) should_send++; else should_send--;
				}
			}

			if (should_send > 0) {
				// send a packet containing the local session magic
				message->addr.Copy(&data[i]->addr);
				message->pkt.type = SyncResponse;
				message->pkt.magic = data[i]->pkt.x.sync_response.rng_data;
				message->pkt.x.sync_response.rng_data = _session_magic;
				//
				_pending_output.push(message);
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
		if (type == PacketType::SyncResponse) {
			auto message = new NetData;
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
				message->addr.Copy(&data[i]->addr);
				message->pkt.type = SyncResponse;
				message->pkt.magic = data[i]->pkt.x.sync_response.rng_data;
				message->pkt.x.sync_response.rng_data = _session_magic;
				//
				_pending_output.push(message);
			}

			delete data[i];
			continue;
		}

		if (type == PacketType::Inputs || type == PacketType::SpectatorInputs) {
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

Gekko::u32 Gekko::MessageSystem::GetMagic()
{
	return _session_magic;
}

std::queue<Gekko::NetInputData*>& Gekko::MessageSystem::LastReceivedInputs()
{
	return _received_inputs;
}

std::vector<Gekko::Handle> Gekko::MessageSystem::GetHandlesForAddress(NetAddress* addr)
{
	auto result = std::vector<Handle>();
	for (auto player: remotes) {
		if (player->address.Equals(*addr))
			result.push_back(player->handle);
	}
	return result;
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

	data->pkt.type = PacketType::Inputs;
	data->pkt.x.input.input_count = (u32)send_list.size();
	data->pkt.x.input.start_frame = last_added - (u32)send_list.size();
	data->pkt.x.input.inputs = (u8*)std::malloc(total_size);
	data->pkt.x.input.total_size = total_size;
	
	if(data->pkt.x.input.inputs)
		std::memcpy(data->pkt.x.input.inputs, inputs.get(), total_size);

	_pending_output.push(data);
}