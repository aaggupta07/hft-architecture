#include "encoded-message.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <span>

namespace exchange{
auto EncodedMessage::serialize() -> const std::span<const std::byte> {
    auto buffer_span = std::span{buffer_}.subspan(0, MessageHeader::PACKED_WIRE_SIZE);
    header_.serialize(buffer_span);
    return message();
}
}
