#pragma once

#include <cstdint>

static constexpr auto EXAMPLE_SERVICE_ADDRESS = "ipc:///tmp/example_service.sock";

namespace ExampleAPIType {
    static constexpr uint32_t ADD = 1;
    static constexpr uint32_t POW2 = 2;
}

struct AddRequest {
    int32_t a;
    int32_t b;
};
struct AddReply {
    int32_t result;
};

struct Pow2Request {
    int32_t x;
};
struct Pow2Reply {
    int32_t result;
};
