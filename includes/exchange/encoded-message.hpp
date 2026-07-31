#ifndef ENCODED_MESSAGE_HPP
#define ENCODED_MESSAGE_HPP

#include "protocol.hpp"
#include "config.hpp"

#include <array>
#include <span>
#include <algorithm>
#include <cassert>

class exchange::EncodedMessage {
public:
	static constexpr size_t MAX_WIRE_SIZE = config::MAX_MESSAGE_BYTES + MessageHeader::PACKED_WIRE_SIZE;
private:
	MessageHeader header_;
	std::array<std::byte, MAX_WIRE_SIZE> buffer_;
	
public:
	EncodedMessage() = default;
    EncodedMessage(MessageHeader header, std::span<std::byte> payload)
        : header_(header)
    { 
    	assert(payload.size() <= config::MAX_MESSAGE_BYTES);
        std::ranges::copy(payload, buffer_.data() + MessageHeader::PACKED_WIRE_SIZE);

    }
    
    const MessageHeader& header() const noexcept {
        return header_;
    }

    const std::span<const std::byte> payload() const noexcept {
		if(header_.sequence_number == 0) return {};
        return std::span{buffer_}.subspan(MessageHeader::PACKED_WIRE_SIZE, header_.payload_length);
    }

	const std::span<const std::byte> message() const noexcept {
		return std::span{buffer_}.first(header_.wire_size());
	}

    const std::span<const std::byte> serialize();

	// Designed for another function (e.g. receivefrom()) to directly fill the buffer
	constexpr std::span<std::byte, MAX_WIRE_SIZE> get_buffer_ref() noexcept {
		return std::span<std::byte, MAX_WIRE_SIZE> {buffer_};
	}

	// Requires a decoded header
	constexpr std::span<std::byte> get_payload_ref() noexcept {
		assert(header_.payload_length <= config::MAX_MESSAGE_BYTES);
		return std::span{buffer_}.subspan(MessageHeader::PACKED_WIRE_SIZE, header_.payload_length);
	}

	void decode_header_from_buffer() noexcept;
};

#endif
