#pragma once

#include "gekko_types.h"

#include <memory>
#include <vector>
#include <chrono>

#include <zpp/serializer.h>

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

    enum PacketType : u8 {
        Inputs = 1,
        SpectatorInputs,
        InputAck,
        SyncRequest,
        SyncResponse,
        HealthCheck,
    };

    struct MsgHeader {
        PacketType type;
        u16 magic;

        template <typename Archive, typename Self>
        static void serialize(Archive& a, Self& s) {
            a(s.type, s.magic);
        }
    };

    struct MsgBody : public zpp::serializer::polymorphic {
    };

    struct InputMsg : MsgBody {
        Frame start_frame;
        u8 input_count;
        u16 total_size;

        std::vector<u8> inputs;

        template <typename Archive, typename Self>
        static void serialize(Archive& a, Self& s) {
            a(s.start_frame, s.input_count, s.total_size, s.inputs);
        }
    };

    struct InputAckMsg : MsgBody {
        Frame ack_frame;
        i8 frame_advantage;

        template <typename Archive, typename Self>
        static void serialize(Archive& a, Self& s) {
            a(s.ack_frame, s.frame_advantage);
        }
    };

    struct SyncMsg : MsgBody {
        u16 rng_data;

        template <typename Archive, typename Self>
        static void serialize(Archive& a, Self& s) {
            a(s.rng_data);
        }
    };

    struct HealthCheckMsg : MsgBody {
        Frame frame;
        u32 checksum;

        template <typename Archive, typename Self>
        static void serialize(Archive& a, Self& s) {
            a(s.frame, s.checksum);
        }
    };

    struct NetPacket {
        MsgHeader header;
        std::unique_ptr<MsgBody> body;
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
        InputMsg input;
    };

    struct NetResult {
        NetAddress addr;
        u32 data_len {};
        std::unique_ptr<char[]> data;
    };

    class NetAdapter {
    public:
        virtual std::vector<std::unique_ptr<NetResult>> ReceiveData() = 0;
        virtual void SendData(NetAddress& addr, const char* data, int length) = 0;
    };

#define GEKKO_USE_DEFAULT_ASIO_SOCKET
#ifdef GEKKO_USE_DEFAULT_ASIO_SOCKET
    class NonBlockingSocket : public NetAdapter {
    };
#endif // USE_DEFAULT_ASIO_SOCKET

}
