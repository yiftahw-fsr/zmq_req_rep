#pragma once

#include <zmq.hpp>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <memory>
#include <type_traits>

struct Header {
    uint32_t endpoint_id;
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
        auto wrapper = [handler = std::move(handler)](zmq::message_t& msg) -> zmq::message_t {
            if (msg.size() != sizeof(Req)) {
                std::cerr << "Payload size mismatch: expected " << sizeof(Req)
                          << ", got " << msg.size() << "\n";
                return zmq::message_t{};
            }

            Req payload;
            std::memcpy(&payload, msg.data(), sizeof(payload));

            Rep reply_struct = handler(payload);

            zmq::message_t reply_msg(sizeof(Rep));
            std::memcpy(reply_msg.data(), &reply_struct, sizeof(Rep));
            return reply_msg;
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
            zmq::message_t header_msg;
            auto recv_result = sock.recv(header_msg, zmq::recv_flags::none);
            if (!recv_result.has_value()) {
                // Timeout or error - check running_ flag and continue
                continue;
            }

            if (header_msg.size() != sizeof(Header)) {
                continue;
            }

            Header hdr;
            std::memcpy(&hdr, header_msg.data(), sizeof(hdr));

            zmq::message_t payload_msg;
            if (!sock.recv(payload_msg, zmq::recv_flags::none)) {
                // Failed to receive payload, skip
                continue;
            }

            zmq::message_t reply_msg;

            auto it = dispatch_table_.find(hdr.endpoint_id);
            if (it != dispatch_table_.end()) {
                reply_msg = it->second(payload_msg);
            } else {
                std::cerr << "No handler for endpoint " << hdr.endpoint_id << "\n";
            }

            sock.send(reply_msg, zmq::send_flags::none);
        }
    }

    std::string addr_;
    std::unordered_map<uint32_t, std::function<zmq::message_t(zmq::message_t&)>> dispatch_table_;
    std::thread server_thread_;
    bool running_ = false;
};
