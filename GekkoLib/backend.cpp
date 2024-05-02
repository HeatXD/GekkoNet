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

void Gekko::NetAddress::Copy(NetAddress& other)
{
	_size = other._size;

	if (_data) 
		_data.reset();

	_data = std::unique_ptr<u8[]>(new u8[_size]);
	// copy address data
	std::memcpy(_data.get(), other.GetAddress(), _size);
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
		_player_input_send_list.push_front(new u8[_input_size * locals.size()]);
		std::memcpy(_player_input_send_list.front(), input, _input_size * locals.size());
	}

	if (_player_input_send_list.size() > MAX_PLAYER_SEND_SIZE) {
		delete _player_input_send_list.back();
		_player_input_send_list.pop_back();
	}
}

void Gekko::MessageSystem::AddSpectatorInput(Frame input_frame, u8 input[])
{
	if (_last_added_spectator_input + 1 == input_frame) {
		_last_added_spectator_input++;
		_spectator_input_send_list.push_front(new u8[_input_size * (locals.size() + remotes.size())]);
		std::memcpy(_spectator_input_send_list.front(), input, _input_size * (locals.size() + remotes.size()));
	}

	if (_spectator_input_send_list.size() > MAX_SPECTATOR_SEND_SIZE) {
		delete _spectator_input_send_list.back();
		_spectator_input_send_list.pop_back();
	}
}

void Gekko::MessageSystem::SendPendingOutput(NetAdapter* host)
{
	if (!host) return;

	// add input packet
	if (!_player_input_send_list.empty()) {
		AddPendingInput(false);
	}

	// add spectator input packet
	if (!_spectator_input_send_list.empty()) {
		AddPendingInput(true);
	}

	// handle messages
	for (u32 i = 0; i < _pending_output.size(); i++) {
		auto pkt = _pending_output.front();
		if (pkt->pkt.type == PacketType::Inputs) {
			for (size_t i = 0; i < remotes.size(); i++) {
				if (remotes[i]->address.GetSize() != 0) {
					// copy addr, set magic and send it off!
					pkt->addr.Copy(remotes[i]->address);
					pkt->pkt.magic = remotes[i]->session_magic;
					host->SendMessage(pkt->addr, pkt->pkt);
				}
			}
			// now when done we can cleanup the inputs since we used malloc
			std::free(pkt->pkt.x.input.inputs);
		}
		else if (pkt->pkt.type == PacketType::SpectatorInputs) {
			for (size_t i = 0; i < spectators.size(); i++) {
				if (spectators[i]->address.GetSize() != 0) {
					// copy addr, set magic and send it off!
					pkt->addr.Copy(spectators[i]->address);
					pkt->pkt.magic = spectators[i]->session_magic;
					host->SendMessage(pkt->addr, pkt->pkt);
				}
			}
			// now when done we can cleanup the inputs since we used malloc
			std::free(pkt->pkt.x.input.inputs);
		}
		else {
			host->SendMessage(pkt->addr, pkt->pkt);
		}
		// cleanup packet
		delete pkt;
		// housekeeping
		_pending_output.pop();
	}
}

void Gekko::MessageSystem::HandleData(std::vector<NetData>& data)
{
	for (u32 i = 0; i < data.size(); i++) {
		// handle connection events
		if (data[i].pkt.type == PacketType::SyncRequest)
		{

			continue;
		}

		if (data[i].pkt.type == PacketType::SyncResponse)
		{
			// TODO
			continue;
		}

		if (data[i].pkt.magic != _session_magic) continue;
		// handle other events

		if (data[i].pkt.type == PacketType::Inputs)
		{
			// TODO
			continue;
		}
	}
}

void Gekko::MessageSystem::AddPendingInput(bool spectator)
{
	const auto& send_list = spectator ? _spectator_input_send_list : _player_input_send_list;

	const Frame last_added = spectator ? _last_added_spectator_input : _last_added_input;

	const u32 num_players = spectator ? locals.size() + remotes.size() : locals.size();
	const u32 input_size = _input_size * num_players;
	const u32 input_count = (u32)send_list.size();
	const u32 total_size = input_size * input_count;

	std::unique_ptr<u8[]> inputs(new u8[total_size]);

	u32 idx = 0;
	for (auto& input : send_list) {
		std::memcpy(&inputs[idx * input_size], input, input_size);
		idx++;
	}

	// TODO? maybe add some type of compression to the inputs
	// the address and magic is set later so dont worry about it now

	auto data = new NetData;

	data->pkt.type = PacketType::Inputs;
	data->pkt.x.input.input_count = (u32)send_list.size();
	data->pkt.x.input.start_frame = last_added - (u32)send_list.size();
	data->pkt.x.input.inputs = (u8*)std::malloc(total_size);
	
	if(data->pkt.x.input.inputs)
		std::memcpy(data->pkt.x.input.inputs, inputs.get(), total_size);

	_pending_output.push(data);
}