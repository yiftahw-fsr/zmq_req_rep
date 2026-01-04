#pragma once

#include <cstdint>

struct RequestHeader {
    uint32_t endpoint_id;
};

struct ReplyHeader {
    uint32_t status_code;
};

// Reply status codes (similar to HTTP)
namespace ReplyStatus {
    static constexpr uint32_t OK = 200;
    static constexpr uint32_t BAD_REQUEST = 400;
    static constexpr uint32_t NOT_FOUND = 404;
    static constexpr uint32_t INTERNAL_ERROR = 500;
}
