#pragma once

#include "ZMQServer.hpp"
#include "ExampleServiceAPI.hpp"

class ExampleService : public ZMQServer {
public:
    ExampleService(const std::string& addr) : ZMQServer(addr) {}
    ~ExampleService() = default;

    void initialize() override;

private:
    AddReply handle_add(const AddRequest& req);
    Pow2Reply handle_pow2(const Pow2Request& req);
};
