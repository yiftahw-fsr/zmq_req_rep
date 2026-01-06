#include "ZMQServer.hpp"
#include "ExampleService.hpp"

void ExampleService::initialize()
{
    // NOTE: it is safe to use 'this' pointer here because 
    // the callbacks are stored privately in this instance,
    // and incoming requests are handled serially in the server thread.
    // The callbacks themselves and their execution will not outlive the server instance.
    on<AddRequest, AddReply>(ExampleAPIType::ADD, [this](const auto &req) { return handle_add(req); });
    on<Pow2Request, Pow2Reply>(ExampleAPIType::POW2, [this](const auto &req) { return handle_pow2(req); });
}

AddReply ExampleService::handle_add(const AddRequest& req) {
    return AddReply{ .result = req.a + req.b };
}

Pow2Reply ExampleService::handle_pow2(const Pow2Request& req) {
    return Pow2Reply{ .result = req.x * req.x };
}
