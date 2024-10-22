#pragma once

#include "gekkonet.h"

#include "gekko_types.h"

#include <memory>
#include <vector>
#include <chrono>

#include "zpp/serializer.h"

#ifndef GEKKONET_NO_ASIO

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif // _WIN32

#include "asio/asio.hpp"

#endif // GEKKONET_NO_ASIO

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

        void Copy(const InputMsg* other) {
            start_frame = other->start_frame;
            input_count = other->input_count;
            total_size = other->total_size;

            inputs.clear();
            inputs.insert(inputs.begin(), other->inputs.begin(), other->inputs.end());
        }

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
        static const u64 DISCONNECT_TIMEOUT = std::chrono::microseconds(2000).count();
        static const u64 SYNC_MSG_DELAY = std::chrono::microseconds(200).count();

        Frame last_acked_frame;
        u64 last_sent_sync_message;
        u64 last_received_message = -1;
    };

    struct NetInputData {
        std::vector<Handle> handles;
        InputMsg input;
    };

    struct NetResult {
        NetAddress addr;
        u32 data_len{};
        std::unique_ptr<char[]> data;
    };

    class NetAdapter {
    public:
        virtual std::vector<std::unique_ptr<NetResult>> ReceiveData() = 0;
        virtual void SendData(NetAddress& addr, const char* data, int length) = 0;
    };

#ifndef GEKKONET_NO_ASIO
    class NonBlockingSocket : public NetAdapter {
    public:
        NonBlockingSocket(u16 port);

        virtual std::vector<std::unique_ptr<NetResult>> ReceiveData();

        virtual void SendData(NetAddress& addr, const char* data, int length);
    private:
        // Utility function to convert ASIO endpoint to string
        std::string ETOS(const asio::ip::udp::endpoint& endpoint) {
            return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
        }

        // Utility function to convert string to ASIO endpoint
        asio::ip::udp::endpoint STOE(const std::string& str) {
            std::string::size_type colon_pos = str.find(':');
            if (colon_pos == std::string::npos) {
                throw std::invalid_argument("Invalid endpoint string");
            }

            std::string address = str.substr(0, colon_pos);
            u16 port = (u16)(std::stoi(str.substr(colon_pos + 1)));

            return asio::ip::udp::endpoint(asio::ip::address::from_string(address), port);
        }
    private:
        char _buffer[1024];

        asio::error_code _ec;

        asio::io_context _io_ctx;

        asio::ip::udp::endpoint _remote;

        std::unique_ptr<asio::ip::udp::socket> _socket;
    };
#endif // !GEKKONET_NO_ASIO
}
