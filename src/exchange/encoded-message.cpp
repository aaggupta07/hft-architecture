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

void EncodedMessage::decode_header_from_buffer() noexcept {
	std::memcpy(&header_.sequence_number, buffer_.data(), sizeof(SequenceID));
	std::memcpy(&header_.payload_length, buffer_.data() + sizeof(SequenceID), sizeof(Length));
	header_.sequence_number = ntohll(header_.sequence_number);
	header_.payload_length = ntohs(header_.payload_length);
}
}
