#include "ZMQServer.hpp"
#include "ExampleService.hpp"

void ExampleService::start()
{
    // Register handlers here
    server.register_handler<AddRequest, AddReply>(ExampleAPIType::ADD, [this](const auto &req) { return handle_add(req); });
    server.register_handler<Pow2Request, Pow2Reply>(ExampleAPIType::POW2, [this](const auto &req) { return handle_pow2(req); });

    // Start server in background
    server.start();
}

void ExampleService::stop()
{
    server.stop();
}
