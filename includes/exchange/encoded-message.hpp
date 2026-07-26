#include "protocol.hpp"
#include <array>
#include <span>
#include <algorithm>
#include <cassert>

class exchange::EncodedMessage {
private: 	MessageHeader header_;
public: 	static constexpr size_t MAX_WIRE_SIZE = MAX_MESSAGE_BYTES + sizeof(header_);
private: 	std::array<Byte, MAX_WIRE_SIZE> buffer_;
	
public:
    EncodedMessage(MessageHeader header, std::span<Byte> payload)
        : header_(header)
    { 
    	assert(payload.size() <= MAX_MESSAGE_BYTES);
        std::ranges::copy(payload, buffer_.data() + sizeof(header_));

    }
    
    const MessageHeader& header() const noexcept {
        return header_;
    }

    const std::span<const Byte> payload() const noexcept {
        return std::span{buffer_}.subspan(sizeof(header_), header_.payload_length);
    }

    std::array<Byte, MAX_WIRE_SIZE> serialize();
};