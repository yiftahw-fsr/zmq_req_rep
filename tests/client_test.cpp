#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <iostream>
#include "ZMQClient.hpp"
#include "ZMQServer.hpp"
#include "ExampleService.hpp"
#include "ExampleServiceAPI.hpp"

using namespace std::chrono_literals;

class ClientTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        service = std::make_unique<ExampleService>(EXAMPLE_SERVICE_ADDRESS);
        service->start();
    }

    void TearDown() override {
        service->stop();
        service.reset();
        std::this_thread::sleep_for(100ms);
    }

    std::unique_ptr<ExampleService> service;
};

TEST_F(ClientTestFixture, AddMode) {
    Header header{ExampleAPIType::ADD};
    AddRequest request{5, 3};
    
    auto result = request_and_wait_response<Header, AddRequest, AddReply>(
        EXAMPLE_SERVICE_ADDRESS, header, request, 1000ms);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().result, 8);
}

TEST_F(ClientTestFixture, Pow2Mode) {
    Header header{ExampleAPIType::POW2};
    Pow2Request request{5};
    
    auto result = request_and_wait_response<Header, Pow2Request, Pow2Reply>(
        EXAMPLE_SERVICE_ADDRESS, header, request, 1000ms);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().result, 25);
}

TEST_F(ClientTestFixture, MultipleRequests) {
    // ADD request
    {
        Header header{ExampleAPIType::ADD};
        AddRequest request{10, 20};
        auto result = request_and_wait_response<Header, AddRequest, AddReply>(
            EXAMPLE_SERVICE_ADDRESS, header, request, 1000ms);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().result, 30);
    }
    
    // POW2 request
    {
        Header header{ExampleAPIType::POW2};
        Pow2Request request{7};
        auto result = request_and_wait_response<Header, Pow2Request, Pow2Reply>(
            EXAMPLE_SERVICE_ADDRESS, header, request, 1000ms);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().result, 49);
    }
}

TEST_F(ClientTestFixture, ServerRecoveryAfterMalformedRequest) {
    // Send a malformed request (header only, no payload)
    try {
        zmq::context_t ctx{1};
        zmq::socket_t sock{ctx, zmq::socket_type::req};
        sock.set(zmq::sockopt::rcvtimeo, 500);
        sock.set(zmq::sockopt::sndtimeo, 500);
        sock.connect(EXAMPLE_SERVICE_ADDRESS);
        
        // Send only header without payload (malformed)
        Header header{ExampleAPIType::POW2};
        zmq::message_t header_msg(&header, sizeof(header));
        sock.send(header_msg, zmq::send_flags::none);
        
        // Try to receive response (may timeout or get empty response)
        zmq::message_t reply;
        const auto recv_result = sock.recv(reply, zmq::recv_flags::none);
        ASSERT_TRUE(!recv_result.has_value() || recv_result.value() == 0);
        std::cout << "Malformed request sent, server should handle it\n";

        // Now send a valid request - server should recover and handle it
        Header header2{ExampleAPIType::ADD};
        AddRequest request{15, 25};
        auto result = request_and_wait_response<Header, AddRequest, AddReply>(
            EXAMPLE_SERVICE_ADDRESS, header2, request, 1000ms);

        ASSERT_TRUE(result.has_value()) << "Server failed to recover after malformed request";
        EXPECT_EQ(result.value().result, 40) << "Server returned wrong result after recovery";
    }
    catch (const zmq::error_t &e)
    {
        FAIL() << "Unexpected ZMQ error after malformed request: " << e.what();
    }
}
