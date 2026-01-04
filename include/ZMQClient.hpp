#pragma once

#include <zmq.hpp>
#include <zmq_addon.hpp>
#include <expected>
#include <chrono>
#include <cstring>
#include <string>
#include <type_traits>
#include <format>
#include <vector>
#include <iostream>
#include "ZMQProtocol.hpp"

/**
 * @brief Send a single request to a REQ/REP endpoint and wait for the reply.
 *
 * This is a **header-only, one-off client** function. It creates a new
 * zmq::context_t and zmq::socket_t on each call. Use this only for
 * single-request synchronous interactions.
 *
 * The `addr` parameter is the full transport address (e.g., "tcp://127.0.0.1:5555"
 * or "ipc:///tmp/mysocket"). The `RequestHeader` type is sent as the first frame
 * and can contain event ID / dispatch key for server-side routing.
 *
 * @tparam RequestHeader Trivially copyable, standard-layout header type
 * @tparam Req Trivially copyable, standard-layout request type
 * @tparam Rep Trivially copyable, standard-layout reply type
 * @param addr Full transport address for the socket (TCP or IPC)
 * @param header RequestHeader object to be sent first (dispatch / event info)
 * @param request Request object to be sent after the header
 * @param timeout Maximum duration to wait for send + receive
 * @return std::expected<Rep, std::string>
 *         - On success: received reply of type Rep
 *         - On failure: error message (timeout or ZMQ error)
 */
template <typename RequestHeader, typename Req, typename Rep>
requires std::is_trivially_copyable_v<RequestHeader> &&
         std::is_trivially_copyable_v<Req> &&
         std::is_trivially_copyable_v<Rep>
std::expected<Rep, std::string> request_and_wait_response(
    const std::string& addr,
    const RequestHeader& header,
    const Req& request,
    std::chrono::milliseconds timeout)
{
    static_assert(std::is_standard_layout_v<RequestHeader>, "RequestHeader must be standard-layout for memcpy");
    static_assert(std::is_standard_layout_v<Req>, "Req must be standard-layout for memcpy");
    static_assert(std::is_standard_layout_v<Rep>, "Rep must be standard-layout for memcpy");

    try {
        zmq::context_t ctx{1};
        zmq::socket_t sock{ctx, zmq::socket_type::req};

        sock.set(zmq::sockopt::rcvtimeo, static_cast<int>(timeout.count()));
        sock.set(zmq::sockopt::sndtimeo, static_cast<int>(timeout.count()));

        sock.connect(addr);

        // Send header as first frame
        zmq::message_t header_msg(&header, sizeof(header));
        sock.send(header_msg, zmq::send_flags::sndmore);

        // Send request as second frame
        zmq::message_t payload_msg(&request, sizeof(request));
        sock.send(payload_msg, zmq::send_flags::none);

        // Receive multipart reply
        std::vector<zmq::message_t> reply_msgs;
        auto recv_result = zmq::recv_multipart(sock, std::back_inserter(reply_msgs));
        
        if (!recv_result.has_value() || reply_msgs.empty()) {
            return std::unexpected(
                std::format("Timeout after {} ms waiting for reply from '{}'", timeout.count(), addr)
            );
        }

        // First part should be ReplyHeader
        if (reply_msgs[0].size() != sizeof(ReplyHeader)) {
            return std::unexpected(
                std::format("Invalid reply header from '{}': expected {} bytes, got {} bytes",
                            addr, sizeof(ReplyHeader), reply_msgs[0].size())
            );
        }

        ReplyHeader reply_hdr;
        std::memcpy(&reply_hdr, reply_msgs[0].data(), sizeof(ReplyHeader));

        // Check status code
        if (reply_hdr.status_code != ReplyStatus::OK) {
            return std::unexpected(
                std::format("Server returned error status {} from '{}'", reply_hdr.status_code, addr)
            );
        }

        // Second part should be the reply payload
        if (reply_msgs.size() < 2) {
            return std::unexpected(
                std::format("Server returned status OK but no payload from '{}'", addr)
            );
        }

        if (reply_msgs[1].size() != sizeof(Rep)) {
            return std::unexpected(
                std::format("Reply payload size mismatch from '{}': expected {} bytes, got {} bytes",
                            addr, sizeof(Rep), reply_msgs[1].size())
            );
        }

        Rep result;
        std::memcpy(&result, reply_msgs[1].data(), sizeof(result));
        return result;

    } catch (const zmq::error_t& e) {
        return std::unexpected(std::format("ZMQ error communicating with '{}': {}", addr, e.what()));
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Unexpected error communicating with '{}': {}", addr, e.what()));
    }
}
