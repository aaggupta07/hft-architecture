#include "protocol.hpp"
#include <array>
#include <span>
#include <algorithm>

class exchange::EncodedMessage {
private:
    const MessageHeader header_;
    std::array<Byte, MAX_MESSAGE_BYTES + sizeof(header_)> buffer_;
public:
    EncodedMessage(MessageHeader header, std::span<Byte> payload)
        : header_(header)
    { 
        std::ranges::copy(payload, buffer_.data() + sizeof(header_));

    }
    
    const MessageHeader& header() const noexcept {
        return header_;
    }

    const std::span<const Byte> payload() const noexcept {
        return std::span{buffer_}.subspan(sizeof(header_), header_.payload_length);
    }

    std::array<Byte, sizeof(header_) + MAX_MESSAGE_BYTES> serialize();
};