#include "backend.h"
#include "input.h"
#include "event.h"
#include "compression.h"

#include <chrono>
#include <cassert>
#include <climits>

// register poly types.
namespace
{
    zpp::serializer::register_types<
        zpp::serializer::make_type<Gekko::SyncMsg, zpp::serializer::make_id("Gekko::SyncMsg")>,
        zpp::serializer::make_type<Gekko::InputMsg, zpp::serializer::make_id("Gekko::InputMsg")>,
        zpp::serializer::make_type<Gekko::InputAckMsg, zpp::serializer::make_id("Gekko::InputAckMsg")>,
        zpp::serializer::make_type<Gekko::SessionHealthMsg, zpp::serializer::make_id("Gekko::SessionHealthMsg")>,
        zpp::serializer::make_type<Gekko::NetworkHealthMsg, zpp::serializer::make_id("Gekko::NetworkHealthMsg")>
    > _;
}

Gekko::MessageSystem::MessageSystem()
{
    _num_players = 0;
	_input_size = 0;
    _last_sent_network_check = 0;

	// gen magic for session
	std::srand((unsigned int)std::time(nullptr));
	_session_magic = std::rand();

	history = AdvantageHistory();
    session_events = SessionEventSystem();
}

void Gekko::MessageSystem::Init(u8 num_players, u32 input_size)
{
    _num_players = num_players;
	_input_size = input_size;

    _net_player_queue.resize(num_players);

	history.Init();
}


void Gekko::MessageSystem::AddInput(Frame input_frame, Handle player, u8 input[], bool remote)
{
    auto& input_q = _net_player_queue[player];
	if (input_q.last_added_input + 1 == input_frame) {
        input_q.last_added_input++;
        input_q.inputs.push_back(std::make_unique<u8[]>(_input_size));
        std::memcpy(input_q.inputs.back().get(), input, _input_size);
        player_input_cache.outdated = true;

		// update history // TODO REDO WITH BACKEND REWORK..
        if (!remote) {
            history.Update(input_frame);
        }
	}

    // move it along discarding old inputs
    // TODO make the local input send queues variable in size by checking diff to last acked input.
	while (input_q.inputs.size() > MAX_INPUT_QUEUE_SIZE) {
        input_q.inputs.pop_front();
	}
}

void Gekko::MessageSystem::AddSpectatorInput(Frame input_frame, u8 input[])
{
    auto& input_q = _net_spectator_queue;
	if (input_q.last_added_input + 1 == input_frame) {
        input_q.last_added_input++;
        const size_t num_players = locals.size() + remotes.size();
        input_q.inputs.push_back(std::make_unique<u8[]>(_input_size * num_players));
        std::memcpy(input_q.inputs.back().get(), input, _input_size * num_players);
        spectator_input_cache.outdated = true;
	}

    // move the input queues along
    if (input_q.inputs.size() > MAX_INPUT_QUEUE_SIZE) {
        input_q.inputs.pop_front();
    }
}

void Gekko::MessageSystem::SendPendingOutput(GekkoNetAdapter* host)
{
	// add input packet
	if (!remotes.empty() && !locals.empty()) {
		AddPendingInput(false);
        // check for disconnects
        HandleTooFarBehindActors(false);
	}

	// add spectator input packet
	if (!spectators.empty() && !locals.empty()) {
		AddPendingInput(true);
        // check for disconnects
        HandleTooFarBehindActors(true);
	}

	// handle messages
	for (u32 i = 0; i < _pending_output.size(); i++) {
		auto& pkt = _pending_output.front();
		if (pkt->pkt.header.type == Inputs || pkt->pkt.header.type == SpectatorInputs) {
            if (pkt->pkt.header.type == Inputs) {
                SendDataToAll(pkt.get(), host);
            } else {
                SendDataToAll(pkt.get(), host, true);
            }
		}
        else if ((pkt->pkt.header.type == SessionHealth || pkt->pkt.header.type == NetworkHealth) && pkt->addr.GetSize() == 0) {
            // send to remotes
            SendDataToAll(pkt.get(), host);
            // send to spectators
            SendDataToAll(pkt.get(), host, true);
        }
		else {
			SendDataTo(pkt.get(), host);
		}
		// housekeeping
		_pending_output.pop();
	}
}

void Gekko::MessageSystem::HandleData(GekkoNetAdapter* host, GekkoNetResult** data, u32 length)
{
    for (u32 i = 0; i < length; i++) {
        auto res = data[i];
        auto addr = NetAddress(res->addr.data, res->addr.size);

        _bin_buffer.clear();

        try {
            _bin_buffer.insert(_bin_buffer.begin(), (u8*)res->data, (u8*)res->data + res->data_len);

            NetPacket pkt;
            zpp::serializer::memory_input_archive in(_bin_buffer);
            in(pkt.header, pkt.body);

            ParsePacket(addr, pkt);
        }
        catch (const std::exception&) {
            printf("failed to deserialize packet\n");
        }

        // cleanup :)
        host->free_data(res->addr.data);
        host->free_data(res->data);
        host->free_data(res);
    }
}

void Gekko::MessageSystem::SendSyncRequest(NetAddress* addr)
{
    if (!addr) {
        return;
    }

    _pending_output.push(std::make_unique<NetData>());
	auto& message = _pending_output.back();

	message->addr.Copy(addr);

	message->pkt.header.type = SyncRequest;
	message->pkt.header.magic = 0;

    auto body = std::make_unique<SyncMsg>();
    body->rng_data = _session_magic;

    message->pkt.body = std::move(body);
}

void Gekko::MessageSystem::SendSyncResponse(NetAddress* addr, u16 magic)
{
    if (!addr || magic == 0) {
        return;
    }

    _pending_output.push(std::make_unique<NetData>());
    auto& message = _pending_output.back();

	message->addr.Copy(addr);
	message->pkt.header.type = SyncResponse;
	message->pkt.header.magic = magic;

    auto body = std::make_unique<SyncMsg>();
    body->rng_data = _session_magic;

    message->pkt.body = std::move(body);
}

void Gekko::MessageSystem::SendInputAck(Handle player, Frame frame)
{
	auto plyr = GetPlayerByHandle(player);

    if (!plyr) {
        return;
    }

    _pending_output.push(std::make_unique<NetData>());
    auto& message = _pending_output.back();

	message->addr.Copy(&plyr->address);
	message->pkt.header.magic = plyr->session_magic;
	message->pkt.header.type = InputAck;

    auto body = std::make_unique<InputAckMsg>();
	body->ack_frame = frame;
	body->frame_advantage = history.GetLocalAdvantage();

    message->pkt.body = std::move(body);
}

std::vector<Handle> Gekko::MessageSystem::GetRemoteHandlesForAddress(NetAddress* addr)
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
    std::vector<std::unique_ptr<Player>>* current = &locals;
    for (u32 i = 0; i < 2; i++)
    {
        if (i == 1) {
            current = &remotes;
        }

        for (auto& player : *current) {
            if (player->handle == handle) {
                return player.get();
            }
        }
    }

	return nullptr;
}

Frame Gekko::MessageSystem::GetMinLastAckedFrame(bool spectator) 
{
	Frame min = INT_MAX;
	for (auto& player : spectator ? spectators : remotes) {
		if (player->GetStatus() == Connected) {
			min = std::min(player->stats.last_acked_frame, min);
		}
	}
	return min;
}

Frame Gekko::MessageSystem::GetLastAddedInput(bool spectator)
{
    if (spectator) return _net_spectator_queue.last_added_input;

    Frame result = INT_MAX;
    for (auto& local : locals) {
        result = std::min(_net_player_queue[local->handle].last_added_input, result);
    }

    return result;
}

bool Gekko::MessageSystem::CheckStatusActors()
{
	i32 result = 0;
	u64 now = TimeSinceEpoch();

    std::vector<std::unique_ptr<Player>>* current = &remotes;

    for (u32 i = 0; i < 2; i++)
    {
        if (i == 1) {
            current = &spectators;
        }
        for (auto& player : *current) {
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
                    else {
                        player->SetStatus(Connected);
                        session_events.AddPlayerConnectedEvent(player->handle);
                        result++;
                    }
                }
                result--;
            }
        }
    }

	return result == 0;
}

void Gekko::MessageSystem::SendSessionHealth(Frame frame, u32 checksum)
{
    _pending_output.push(std::make_unique<NetData>());
    auto& message = _pending_output.back();

    // the address and magic is set later so dont worry about it now
    message->pkt.header.type = SessionHealth;

    auto body = std::make_unique<SessionHealthMsg>();
    body->frame = frame;
    body->checksum = checksum;

    message->pkt.body = std::move(body);
}

void Gekko::MessageSystem::SendNetworkHealth()
{
    u64 now = TimeSinceEpoch();

    // dont want to spam the network with network health packets
    if (_last_sent_network_check + NetStats::NET_CHECK_DELAY > now) {
        return;
    }

    _pending_output.push(std::make_unique<NetData>());
    auto& message = _pending_output.back();

    // the address and magic is set later so dont worry about it now
    message->pkt.header.type = NetworkHealth;

    auto body = std::make_unique<NetworkHealthMsg>();
    body->send_time = now;
    body->received = false;

    message->pkt.body = std::move(body);

    _last_sent_network_check = now;
}

Frame Gekko::MessageSystem::GetLastAddedInputFrom(Handle player)
{
    return _net_player_queue[player].last_added_input;
}

std::deque<std::unique_ptr<u8[]>>& Gekko::MessageSystem::GetNetPlayerQueue(Handle player)
{
    return _net_player_queue[player].inputs;
}

void Gekko::MessageSystem::HandleTooFarBehindActors(bool spectator)
{
    const u64 now = TimeSinceEpoch();
	for (auto& actor : spectator ? spectators : remotes) {
		if (actor->GetStatus() == Connected) {
            // give the actor a chance to save itself.
            if (actor->stats.last_received_message == 0) {
                actor->stats.last_received_message = now;
                continue;
            }
            // check whether messages are being sent if not disconnect.
            const u64 msg_diff = now - actor->stats.last_received_message;
			if (msg_diff > NetStats::DISCONNECT_TIMEOUT) {
                session_events.AddPlayerDisconnectedEvent(actor->handle);
                actor->SetStatus(Disconnected);
                actor->sync_num = 0;
			}
		}
	}
}

u64 Gekko::MessageSystem::TimeSinceEpoch()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void Gekko::MessageSystem::SendDataToAll(NetData* pkt, GekkoNetAdapter* host, bool spectators_only)
{
    auto& actors = spectators_only ? spectators : remotes;

    std::vector<u8> body_buffer;

    try {
        zpp::serializer::memory_output_archive out(body_buffer);
        out(pkt->pkt.body);
    }
    catch (const std::exception&)
    {
        printf("failed to serialize packet body\n");
        return;
    }


    for (auto& actor : actors) {
        _bin_buffer.clear();
        if (actor->address.GetSize() != 0 && actor->GetStatus() != Disconnected) {

            pkt->addr.Copy(&actor->address);
            pkt->pkt.header.magic = actor->session_magic;

            try {
                zpp::serializer::memory_output_archive out(_bin_buffer);
                out(pkt->pkt.header);
            }
            catch (const std::exception&)
            {
                printf("failed to serialize packet header\n");
                continue;
            }

            _bin_buffer.insert(
                _bin_buffer.end(),
                body_buffer.begin(),
                body_buffer.end()
            );

            auto addr = GekkoNetAddress();
            addr.data = actor->address.GetAddress();
            addr.size = actor->address.GetSize();

            host->send_data(&addr, (char*)_bin_buffer.data(), (int)_bin_buffer.size());
        }
    }
}

void Gekko::MessageSystem::SendDataTo(NetData* pkt, GekkoNetAdapter* host)
{
    _bin_buffer.clear();

    try {
        zpp::serializer::memory_output_archive out(_bin_buffer);
        out(pkt->pkt.header, pkt->pkt.body);
    }
    catch (const std::exception&)
    {
        printf("failed to serialize packet\n");
        return;
    }

    auto addr = GekkoNetAddress();
    addr.data = pkt->addr.GetAddress();
    addr.size = pkt->addr.GetSize();

    host->send_data(&addr, (char*)_bin_buffer.data(), (int)_bin_buffer.size());
}

void Gekko::MessageSystem::ParsePacket(NetAddress& addr, NetPacket& pkt)
{
    u64 now = TimeSinceEpoch();
    // update receive timers.
    std::vector<std::unique_ptr<Player>>* current = &remotes;
    for (u32 i = 0; i < 2; i++)
    {
        if (i == 1) {
            current = &spectators;
        }

        for (auto& player : *current) {
            if (player->address.Equals(addr)) {
                player->stats.last_received_message = now;
            }
        }
    }

    // handle packet.
    if (pkt.header.magic != _session_magic) {
        if (pkt.header.type == SyncRequest) {
            OnSyncRequest(addr, pkt);
        }
        else {
            printf("dropped packet!\n");
        }
    }
    else {
        switch (pkt.header.type)
        {
        case SyncResponse:
            OnSyncResponse(addr, pkt);
            return;
        case Inputs:
        case SpectatorInputs:
            OnInputs(addr, pkt);
            return;
        case InputAck:
            OnInputAck(addr, pkt);
            return;
        case SessionHealth:
            OnSessionHealth(addr, pkt);
            return;
        case NetworkHealth:
            OnNetworkHealth(addr, pkt);
            return;
        default:
            assert(false && "cannot process an unknown event!");
            return;
        }
    }
}

void Gekko::MessageSystem::OnSyncRequest(NetAddress& addr, NetPacket& pkt)
{
    i32 should_send = 0;
    u64 now = TimeSinceEpoch();
    auto body = (SyncMsg*)pkt.body.get();

    // handle requests and set the peer its session magic for both remotes and spectators
    std::vector<std::unique_ptr<Player>>* current = &remotes;
    for (u32 i = 0; i < 2; i++)
    {
        if (i == 1) {
            current = &spectators;
        }

        for (auto& player : *current) {
            if (player->address.Equals(addr)) {
                player->session_magic = body->rng_data;
                if (player->sync_num == 0) {
                    player->stats.last_sent_sync_message = now;
                    should_send++;
                }
            }
        }
    }

    if (should_send > 0) {
	    // send a packet containing the local session magic
	    SendSyncResponse(&addr, body->rng_data);
    }
}

void Gekko::MessageSystem::OnSyncResponse(NetAddress& addr, NetPacket& pkt)
{
    i32 should_send = 0;
    u64 now = TimeSinceEpoch();
    auto body = (SyncMsg*)pkt.body.get();

    // handle sync responses for both remotes and spectators
    std::vector<std::unique_ptr<Player>>* current = &remotes;
    for (u32 i = 0; i < 2; i++)
    {
        if (i == 1) {
            current = &spectators;
        }

        for (auto& player : *current) {
            if (player->GetStatus() == Connected) {
                // connected but the remote is still asking ? maybe high packet loss? send a response again
                should_send++;
                continue;
            }

            if (player->address.Equals(addr)) {
                player->session_magic = body->rng_data;
                if (player->sync_num < NUM_TO_SYNC) {
                    player->sync_num++;
                    should_send++;
                    player->stats.last_sent_sync_message = now;
                    session_events.AddPlayerSyncingEvent(
                        player->handle,
                        player->sync_num,
                        NUM_TO_SYNC
                    );
                    continue;
                }

                if (player->sync_num >= NUM_TO_SYNC) {
                    player->SetStatus(Connected);
                    session_events.AddPlayerConnectedEvent(player->handle);
                    continue;
                }
            }
        }
    }

    if (should_send > 0) {
    	// send a packet containing the local session magic
    	SendSyncResponse(&addr, body->rng_data);
    }
}

void Gekko::MessageSystem::OnInputs(NetAddress& addr, NetPacket& pkt)
{
    auto body = (InputMsg*)pkt.body.get();

    const Frame start_frame = body->start_frame;
    const u32 input_count = body->input_count;
    const Frame end_frame = start_frame + input_count;

    const bool is_spectator = (pkt.header.type == SpectatorInputs);

    if (is_spectator) {
        for (u32 frame_idx = 0; frame_idx < input_count; frame_idx++) {
            const Frame recv_frame = start_frame + frame_idx;
            const u32 frame_offset = frame_idx * _num_players * _input_size;

            for (u32 player = 0; player < _num_players; player++) {
                u8* input = &body->inputs[frame_offset + player * _input_size];
                AddInput(recv_frame, player, input, true);
            }
        }
    } else {
        auto handles = GetRemoteHandlesForAddress(&addr);
        const u32 player_count = (u32)handles.size();

        for (u32 i = 0; i < player_count; i++) {
            const u32 player_offset = i * input_count * _input_size;

            for (u32 frame_idx = 0; frame_idx < input_count; frame_idx++) {
                const Frame recv_frame = start_frame + frame_idx;
                u8* input = &body->inputs[player_offset + frame_idx * _input_size];
                AddInput(recv_frame, handles[i], input, true);
            }

            auto player = GetPlayerByHandle(handles[i]);
            if (player) {
                player->stats.last_received_frame = TimeSinceEpoch();
            }
        }
    }
}

void Gekko::MessageSystem::OnInputAck(NetAddress& addr, NetPacket& pkt)
{
    auto body = (InputAckMsg*)pkt.body.get();
    // we should just update the ack frame for all handles where the address matches
	const Frame ack_frame = body->ack_frame;
    const i32 remote_advantage = body->frame_advantage;
    bool added_advantage = false;

    std::vector<std::unique_ptr<Player>>* current = &remotes;
    for (u32 i = 0; i < 2; i++)
    {
        if (i == 1) {
            current = &spectators;
        }

        for (auto& player : *current) {
            if (player->address.Equals(addr)) {
                if (player->stats.last_acked_frame < ack_frame) {
                    player->stats.last_acked_frame = ack_frame;
                    // only add remote advantages once
                    if (!added_advantage && i == 0) {
                        history.AddRemoteAdvantage(remote_advantage);
                        added_advantage = true;
                    }
                }
            }
        }
    }
}

void Gekko::MessageSystem::OnSessionHealth(NetAddress& addr, NetPacket& pkt)
{
    auto body = (SessionHealthMsg*)pkt.body.get();

    const Frame frame = body->frame;
    const u32 checksum = body->checksum;

    for (auto& player : remotes) {
        if (player->address.Equals(addr)) {
            player->SetChecksum(frame, checksum);

            for (auto iter = player->session_health.begin();
                iter != player->session_health.end(); ) {
                if (iter->first < (_net_player_queue[player->handle].last_added_input - 128)) {
                    iter = player->session_health.erase(iter);
                } else {
                    ++iter;
                }
            }
            break;
        }
    }
}

void Gekko::MessageSystem::OnNetworkHealth(NetAddress& addr, NetPacket& pkt)
{
    auto body = (NetworkHealthMsg*)pkt.body.get();

    // ok if its not a returned packet then update it and send it back to its specifc peer.
    if (!body->received) {
        _pending_output.push(std::make_unique<NetData>());
        auto& message = _pending_output.back();

        auto player = GetPlayerByHandle(GetRemoteHandlesForAddress(&addr).at(0));

        message->pkt.header.magic = player->session_magic;
        message->pkt.header.type = NetworkHealth;

        auto new_body = std::make_unique<NetworkHealthMsg>();
        new_body->send_time = body->send_time;
        new_body->received = true;

        message->pkt.body = std::move(new_body);
        message->addr.Copy(&addr);
        return;
    }

    // else update network stats
    u16 rtt_ms = (u16)(TimeSinceEpoch() - body->send_time);
    std::vector<std::unique_ptr<Player>>* current = &remotes;

    for (u32 i = 0; i < 2; i++)
    {
        if (i == 1) {
            current = &spectators;
        }

        for (auto& actor : *current) {
            // add rtt times to a list 
            if (addr.Equals(actor->address)) {
                actor->stats.rtt.push_back(rtt_ms);
            }
            // cleanup
            if (actor->stats.rtt.size() > 10) {
                actor->stats.rtt.erase(actor->stats.rtt.begin());
            }
        }
    }
}

void Gekko::MessageSystem::AddPendingInput(bool spectator)
{
    const u32 MAX_INPUT_SIZE = 512;

    const auto packet_type = spectator ? SpectatorInputs : Inputs;
    auto& input_cache = spectator ? spectator_input_cache : player_input_cache;
    auto& queue = spectator ? _net_spectator_queue : _net_player_queue[locals[0]->handle];

    const u32 num_players = spectator ? _num_players : (u32)locals.size();
    const u32 inputs_per_packet = MAX_INPUT_SIZE / (_input_size * num_players);
    const u32 q_size = (u32)queue.inputs.size();

    if (q_size == 0) return;

    // check if cache is valid and can be reused
    if (!input_cache.outdated && !input_cache.input.empty()) {
        // reuse cached packets, just copy from cache
        for (const auto& cached_msg : input_cache.input) {
            _pending_output.push(std::make_unique<NetData>());
            auto& packet = _pending_output.back();
            packet->pkt.header.type = packet_type;
            auto message = std::make_unique<InputMsg>();
            message->Copy(&cached_msg);
            packet->pkt.body = std::move(message);
        }
        return; 
    }

    // cache is outdated, rebuild packets
    input_cache.input.clear();

    const u32 packet_count = (q_size + inputs_per_packet - 1) / inputs_per_packet;
    const Frame start_frame = queue.last_added_input - q_size + 1;

    for (u32 pc = 0; pc < packet_count; pc++) {
        const u32 input_start_idx = pc * inputs_per_packet;
        const u32 input_end_idx = std::min(q_size, input_start_idx + inputs_per_packet);
        const u32 input_count = input_end_idx - input_start_idx;

        _pending_output.push(std::make_unique<NetData>());
        auto& packet = _pending_output.back();
        packet->pkt.header.type = packet_type;
        auto message = std::make_unique<InputMsg>();
        message->start_frame = start_frame + input_start_idx;

        if (spectator) {
            for (u32 i = input_start_idx; i < input_end_idx; i++) {
                const auto& p_input = queue.inputs.at(i);
                message->inputs.insert(message->inputs.end(),
                    p_input.get(),
                    p_input.get() + _input_size * num_players);
            }
        }
        else {
            for (u32 player = 0; player < num_players; player++) {
                const auto& player_queue = _net_player_queue[locals[player]->handle];
                for (u32 i = input_start_idx; i < input_end_idx; i++) {
                    const auto& p_input = player_queue.inputs.at(i);
                    message->inputs.insert(message->inputs.end(),
                        p_input.get(),
                        p_input.get() + _input_size);
                }
            }
        }

        message->total_size = (u16)message->inputs.size();
        message->input_count = input_count;

        // cache this message before moving it
        InputMsg cached_msg;
        cached_msg.Copy(message.get());
        input_cache.input.push_back(std::move(cached_msg));

        packet->pkt.body = std::move(message);
    }
    // mark cache as valid
    input_cache.outdated = false;
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

	i32 sum = 0;
	for (i8 num : _remote_frame_adv) {
        sum += num;
	}

    sum /= HISTORY_SIZE;
    _remote[update_frame % HISTORY_SIZE] = sum;
}

f32 Gekko::AdvantageHistory::GetAverageAdvantage()
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
	return (avg_local - avg_remote);
}

void Gekko::AdvantageHistory::SetLocalAdvantage(i8 adv) {
	_local_frame_adv = adv;
}

void Gekko::AdvantageHistory::AddRemoteAdvantage(i8 adv) {
    _remote_frame_adv[_adv_index % HISTORY_SIZE] = adv;
    _adv_index++;
}

i8 Gekko::AdvantageHistory::GetLocalAdvantage() {
	return _local_frame_adv;
}
