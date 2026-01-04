#pragma once

#include "ZMQServer.hpp"
#include "ExampleServiceAPI.hpp"

class ExampleService {
public:
    ExampleService(const std::string& addr) : server(addr) {}
    ~ExampleService() { stop(); }

    void start();
    void stop();

private:
    AddReply handle_add(const AddRequest& req) {
        AddReply rep{0};
        rep.result = req.a + req.b;
        return rep;
    }

    Pow2Reply handle_pow2(const Pow2Request& req) {
        Pow2Reply rep{0};
        rep.result = req.x * req.x;
        return rep;
    }

    ZMQServer server;
};
