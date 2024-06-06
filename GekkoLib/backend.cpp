#include "backend.h"
#include "input.h"
#include "event.h"

#include <chrono>

Gekko::MessageSystem::MessageSystem()
{
	_input_size = 0;
	_last_added_input = GameInput::NULL_FRAME;
	_last_added_spectator_input = GameInput::NULL_FRAME;

	// gen magic for session
	std::srand(std::time(nullptr));
	_session_magic = std::rand();

	history = AdvantageHistory();
    session_events = SessionEventSystem();
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
		if (pkt->pkt.type == Inputs || pkt->pkt.type == SpectatorInputs) {
            if (pkt->pkt.type == Inputs) {
                SendDataToAll(pkt, host);
            } else {
                SendDataToAll(pkt, host, true);
            }
			// now when done we can cleanup the inputs since we used malloc
			std::free(pkt->pkt.x.input.inputs);
		}
        else if (pkt->pkt.type == HealthCheck) {
            // send to remotes
            SendDataToAll(pkt, host);
            // send to spectators
            SendDataToAll(pkt, host, true);
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
                        session_events.AddPlayerSyncingEvent(remotes[j]->handle, remotes[j]->sync_num, NUM_TO_SYNC);
						continue;
					}

					if (remotes[j]->sync_num == NUM_TO_SYNC) {
						remotes[j]->SetStatus(Connected);
						stop_sending++;
                        session_events.AddPlayerConnectedEvent(remotes[j]->handle);
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
                        session_events.AddPlayerSyncingEvent(spectators[j]->handle, spectators[j]->sync_num, NUM_TO_SYNC);
						continue;
					}

					if (spectators[j]->sync_num == NUM_TO_SYNC) {
						spectators[j]->SetStatus(Connected);
						stop_sending++;
                        session_events.AddPlayerConnectedEvent(spectators[j]->handle);
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

            if (net_input->input.inputs) {
                std::memcpy(net_input->input.inputs, data[i]->pkt.x.input.inputs, net_input->input.total_size);
            }

			// now when done we can cleanup the inputs since we used malloc
			std::free(data[i]->pkt.x.input.inputs);

			_received_inputs.push(net_input);

			delete data[i];
			continue;
		}

		if(type == InputAck) {
			// we should just update the ack frame for all handles where the address matches
			const Frame ack_frame = data[i]->pkt.x.input_ack.ack_frame;
            const i32 remote_advantage = data[i]->pkt.x.input_ack.frame_advantage;
            bool added_advantage = false;

			for (auto& player : remotes) {
				if (player->address.Equals(data[i]->addr)) {
					if (player->stats.last_acked_frame < ack_frame) {
						player->stats.last_acked_frame = ack_frame;
                        // only add remote advantages once
                        if (!added_advantage) {
                            history.AddRemoteAdvantage(remote_advantage);
                            added_advantage = true;
                        }
					}
				}
			}

			for (auto& player : spectators) {
				if (player->address.Equals(data[i]->addr)) {
					if (player->stats.last_acked_frame < ack_frame) {
						player->stats.last_acked_frame = ack_frame;
					}
				}
			}

			delete data[i];
			continue;
		}

        if (type == HealthCheck) {
            // printf("recv addr:%d, f: %d, check:%u\n", *data[i]->addr.GetAddress(), data[i]->pkt.x.health_check.frame, data[i]->pkt.x.health_check.checksum);
            const Frame frame = data[i]->pkt.x.health_check.frame;
            const u32 checksum = data[i]->pkt.x.health_check.checksum;

            for (auto& player : remotes) {
                if (player->address.Equals(data[i]->addr)) {
                    player->SetChecksum(frame, checksum);
                    for (auto& entry : local_health) {
                        if (entry.frame == frame && entry.checksum != checksum) {
                            session_events.AddDesyncDetectedEvent(
                                frame,player->handle, entry.checksum, checksum);
                        }
                    }
                    break;
                }
            }

            delete data[i];
            continue;
        }
	}
}

void Gekko::MessageSystem::SendSyncRequest(NetAddress* addr)
{
    if (!addr) {
        return;
    }

	auto message = new NetData;

	message->addr.Copy(addr);
	message->pkt.type = SyncRequest;
	message->pkt.magic = 0;
	message->pkt.x.sync_request.rng_data = _session_magic;

	_pending_output.push(message);
}

void Gekko::MessageSystem::SendSyncResponse(NetAddress* addr, u32 magic)
{
    if (!addr || magic == 0) {
        return;
    }

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

    if (!plyr) {
        return;
    }

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
	for (auto& player: remotes) {
		if (player->address.Equals(*addr)) {
			result.push_back(player->handle);
		}
	}
	return result;
}

Gekko::Player* Gekko::MessageSystem::GetPlayerByHandle(Handle handle) 
{
	for (auto& player: remotes) {
		if (player->handle == handle) {
			return player.get();
		}
	}
	return nullptr;
}

Gekko::Frame Gekko::MessageSystem::GetMinLastAckedFrame(bool spectator) 
{
	Frame min = INT_MAX;
	for (auto& player : spectator ? spectators : remotes) {
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

	for (auto& player : remotes) {
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

	for (auto& player : spectators) {
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

void Gekko::MessageSystem::SendHealthCheck(Frame frame, u32 checksum)
{
    auto data = new NetData;

    // the address and magic is set later so dont worry about it now
    data->pkt.type = HealthCheck;
    data->pkt.x.health_check.frame = frame;
    data->pkt.x.health_check.checksum = checksum;

    _pending_output.push(data);
}

void Gekko::MessageSystem::HandleTooFarBehindActors(bool spectator)
{
	const u32 max_diff = spectator ? MAX_SPECTATOR_SEND_SIZE : MAX_PLAYER_SEND_SIZE;
	const Frame last_added = spectator ? _last_added_spectator_input : _last_added_input;

	for (auto& player : spectator ? spectators : remotes) {
		if (player->GetStatus() == Connected) {
			const u32 diff = last_added - player->stats.last_acked_frame;
			if (diff > max_diff) {
                session_events.AddPlayerDisconnectedEvent(player->handle);
                player->SetStatus(Disconnected);
                player->sync_num = 0;
			}
		}
	}
}

Gekko::u64 Gekko::MessageSystem::TimeSinceEpoch()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void Gekko::MessageSystem::SendDataToAll(NetData* pkt, NetAdapter* host, bool spectators_only)
{
    auto& actors = spectators_only ? spectators : remotes;

    for (auto& actor : actors) {
        if (actor->address.GetSize() != 0) {
            pkt->addr.Copy(&actor->address);
            pkt->pkt.magic = actor->session_magic;
            host->SendData(pkt->addr, pkt->pkt);
        }
    }
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
	
    if (data->pkt.x.input.inputs) {
        std::memcpy(data->pkt.x.input.inputs, inputs.get(), total_size);
    }

	_pending_output.push(data);
}

void Gekko::AdvantageHistory::Init()
{
    _adv_index = 0;
	_local_frame_adv = 0;

    std::memset(_remote_frame_adv, 0, HISTORY_SIZE * sizeof(i8));
	std::memset(_local, 0, HISTORY_SIZE * sizeof(i8));
	std::memset(_remote, 0, HISTORY_SIZE * sizeof(i8));
}

void Gekko::AdvantageHistory::Update(Frame frame)
{
	const u32 update_frame = std::max(frame, 0);

	_local[update_frame % HISTORY_SIZE] = _local_frame_adv;

	i8 max = INT8_MIN;
	for (i8 num : _remote_frame_adv) {
		max = std::max(max, num);
	}
	_remote[update_frame % HISTORY_SIZE] = max == INT8_MIN ? 0 : max;
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

	// return the frames ahead
	return (avg_local - avg_remote) / 2.f;
}

void Gekko::AdvantageHistory::SetLocalAdvantage(i8 adv) {
	_local_frame_adv = adv;
}

void Gekko::AdvantageHistory::AddRemoteAdvantage(i8 adv) {
    _remote_frame_adv[_adv_index % HISTORY_SIZE] = adv;
    _adv_index++;
}

Gekko::i8 Gekko::AdvantageHistory::GetLocalAdvantage() {
	return _local_frame_adv;
}
