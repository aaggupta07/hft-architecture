#include "encoded-message.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <span>

namespace exchange{
auto EncodedMessage::serialize() -> std::array<Byte, sizeof(header_) + MAX_MESSAGE_BYTES> {
    auto buffer_span = std::span{buffer_}.subspan(0, sizeof(header_));
    header_.serialize(buffer_span);
    return buffer_;
}
}