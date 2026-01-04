#pragma once

#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <memory>
#include <type_traits>
#include "ZMQProtocol.hpp"

struct ServerReply {
    zmq::message_t header;
    zmq::message_t payload;
    bool has_payload;
    
    ServerReply(const ReplyHeader& reply_hdr) : has_payload(false) {
        header = zmq::message_t(sizeof(ReplyHeader));
        std::memcpy(header.data(), &reply_hdr, sizeof(ReplyHeader));
    }
    
    template<typename T>
    ServerReply(const ReplyHeader& reply_hdr, const T& payload_data) : has_payload(true) {
        header = zmq::message_t(sizeof(ReplyHeader));
        std::memcpy(header.data(), &reply_hdr, sizeof(ReplyHeader));
        
        payload = zmq::message_t(sizeof(T));
        std::memcpy(payload.data(), &payload_data, sizeof(T));
    }
};

class ZMQServer {
public:
    ZMQServer(const std::string& addr)
        : addr_(addr) {}

    ~ZMQServer() {
        stop();
    }

    // Register handler
    template <typename Req, typename Rep>
    requires std::is_trivially_copyable_v<Req> &&
             std::is_standard_layout_v<Req> &&
             std::is_trivially_copyable_v<Rep> &&
             std::is_standard_layout_v<Rep>
    void register_handler(uint32_t endpoint_id, std::function<Rep(Req)> handler)
    {
        // Type erasure wrapper to have a homogeneous dispatch table
        auto wrapper = [handler = std::move(handler)](std::vector<zmq::message_t>& msgs) -> ServerReply {
            if (msgs.size() != 2) {
                std::cerr << "Expected 2 message parts, got " << msgs.size() << "\n";
                return ServerReply(ReplyHeader{ReplyStatus::BAD_REQUEST});
            }

            // First message is header
            if (msgs[0].size() != sizeof(RequestHeader)) {
                std::cerr << "RequestHeader size mismatch: expected " << sizeof(RequestHeader)
                          << ", got " << msgs[0].size() << "\n";
                return ServerReply(ReplyHeader{ReplyStatus::BAD_REQUEST});
            }

            // Second message is payload
            if (msgs[1].size() != sizeof(Req)) {
                std::cerr << "Payload size mismatch: expected " << sizeof(Req)
                          << ", got " << msgs[1].size() << "\n";
                return ServerReply(ReplyHeader{ReplyStatus::BAD_REQUEST});
            }

            Req payload;
            std::memcpy(&payload, msgs[1].data(), sizeof(payload));

            Rep reply_struct = handler(payload);

            return ServerReply(ReplyHeader{ReplyStatus::OK}, reply_struct);
        };

        dispatch_table_[endpoint_id] = std::move(wrapper);
    }

    // Start server in a thread
    void start() {
        running_ = true;
        server_thread_ = std::thread([this]() { this->run_loop(); });
    }

    // Stop server and join thread
    void stop() {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

protected:
    void run_loop() {
        zmq::context_t ctx{1};
        zmq::socket_t sock{ctx, zmq::socket_type::rep};
        sock.bind(addr_);

        // Set receive timeout to periodically check running_ flag
        sock.set(zmq::sockopt::rcvtimeo, 100);  // 100ms timeout

        while (running_) {
            std::vector<zmq::message_t> msgs;
            auto recv_result = zmq::recv_multipart(sock, std::back_inserter(msgs));
            
            if (!recv_result.has_value()) {
                // Timeout or error - check running_ flag and continue
                continue;
            }

            if (msgs.empty()) {
                std::cerr << "Received empty message\n";
                continue;
            }

            // First part should be the header
            if (msgs.size() < 1 || msgs[0].size() != sizeof(RequestHeader)) {
                std::cerr << "Invalid message format\n";
                auto error_reply = ServerReply(ReplyHeader{ReplyStatus::BAD_REQUEST});
                sock.send(error_reply.header, zmq::send_flags::none);
                continue;
            }

            RequestHeader hdr;
            std::memcpy(&hdr, msgs[0].data(), sizeof(hdr));

            ServerReply reply(ReplyHeader{ReplyStatus::NOT_FOUND});

            auto it = dispatch_table_.find(hdr.endpoint_id);
            if (it != dispatch_table_.end()) {
                reply = it->second(msgs);
            } else {
                std::cerr << "No handler for endpoint " << hdr.endpoint_id << "\n";
            }

            // Send reply (header always, payload if present)
            if (reply.has_payload) {
                sock.send(reply.header, zmq::send_flags::sndmore);
                sock.send(reply.payload, zmq::send_flags::none);
            } else {
                sock.send(reply.header, zmq::send_flags::none);
            }
        }
    }

    std::string addr_;
    std::unordered_map<uint32_t, std::function<ServerReply(std::vector<zmq::message_t>&)>> dispatch_table_;
    std::thread server_thread_;
    bool running_ = false;
};
