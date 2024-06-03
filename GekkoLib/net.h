#pragma once

#include "gekko_types.h"

#include <memory>
#include <vector>
#include <chrono>

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
            struct InputAck {
                Frame ack_frame;
                i8 frame_advantage;
            } input_ack;
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
        static const u64 SYNC_MSG_DELAY = std::chrono::microseconds(200).count();
        Frame last_acked_frame;
        u64 last_sent_sync_message;
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
}