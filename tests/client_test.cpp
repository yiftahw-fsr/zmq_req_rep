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
        std::cout << "Service started, waiting briefly for it to initialize...\n";
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
