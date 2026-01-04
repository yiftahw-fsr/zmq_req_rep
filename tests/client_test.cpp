#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <zmq_addon.hpp>
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
    RequestHeader header{ExampleAPIType::ADD};
    AddRequest request{5, 3};
    
    auto result = request_and_wait_response<RequestHeader, AddRequest, AddReply>(
        EXAMPLE_SERVICE_ADDRESS, header, request, 1000ms);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().result, 8);
}

TEST_F(ClientTestFixture, Pow2Mode) {
    RequestHeader header{ExampleAPIType::POW2};
    Pow2Request request{5};
    
    auto result = request_and_wait_response<RequestHeader, Pow2Request, Pow2Reply>(
        EXAMPLE_SERVICE_ADDRESS, header, request, 1000ms);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().result, 25);
}

TEST_F(ClientTestFixture, MultipleRequests) {
    // ADD request
    {
        RequestHeader header{ExampleAPIType::ADD};
        AddRequest request{10, 20};
        auto result = request_and_wait_response<RequestHeader, AddRequest, AddReply>(
            EXAMPLE_SERVICE_ADDRESS, header, request, 1000ms);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().result, 30);
    }
    
    // POW2 request
    {
        RequestHeader header{ExampleAPIType::POW2};
        Pow2Request request{7};
        auto result = request_and_wait_response<RequestHeader, Pow2Request, Pow2Reply>(
            EXAMPLE_SERVICE_ADDRESS, header, request, 1000ms);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value().result, 49);
    }
}

TEST_F(ClientTestFixture, ServerRecoveryAfterMalformedRequest) {
    // Define empty structs to simulate malformed/invalid payload
    struct EmptyRequest {};
    struct EmptyReply {};
    
    // Send a malformed request using the API with empty payload
    RequestHeader header{ExampleAPIType::ADD};
    EmptyRequest empty_request{};
    auto malformed_result = request_and_wait_response<RequestHeader, EmptyRequest, EmptyReply>(
        EXAMPLE_SERVICE_ADDRESS, header, empty_request, 500ms);
    
    // Should fail with BAD_REQUEST error
    ASSERT_FALSE(malformed_result.has_value()) << "Malformed request should fail";
    
    std::cout << "Malformed request rejected with error: " << malformed_result.error() << "\n";
    
    // Give server a moment to be ready for next request
    std::this_thread::sleep_for(100ms);
    
    // Now send a valid request - server should recover and handle it
    RequestHeader header2{ExampleAPIType::ADD};
    AddRequest request{15, 25};
    auto result = request_and_wait_response<RequestHeader, AddRequest, AddReply>(
        EXAMPLE_SERVICE_ADDRESS, header2, request, 1000ms);

    ASSERT_TRUE(result.has_value()) << "Server failed to recover after malformed request";
    EXPECT_EQ(result.value().result, 40) << "Server returned wrong result after recovery";
}
