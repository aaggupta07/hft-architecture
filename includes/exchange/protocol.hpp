#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include "order.hpp"

#include <cstdint>
#include <cstddef>
#include <span>
#include <concepts>

namespace exchange {
using SequenceID = uint64_t;
using Length = uint16_t;

// A sequence number of 0 is reserved for error messages
// In this case, the payload_length field contains the error code, and the buffer is empty
struct MessageHeader {
    SequenceID sequence_number;
    Length payload_length;

    void serialize(std::span<std::byte> buffer) const;
};

struct OrderRequest {
	enum class Type: uint8_t {
		Buy,
		Sell,
		Cancel,
	};

	Order::ID order_id;			// Used for CANCEL requests
	Order::Price price;			// Used for BUY/SELL requests
	Order::Quantity quantity; 	// Used for BUY/SELL requests
	Type type;
};

class MarketRequestGenerator;
class CentralLimitOrderBook;

class BinaryOrderExchangeFormat;
class EncodedMessage;

template<typename T>
concept ConvertibleToSpan = requires {
	typename std::ranges::range_value_t<T>;
} && (
	std::convertible_to<T, std::span<std::ranges::range_value_t<T>>> ||
	std::convertible_to<T, std::span<const std::ranges::range_value_t<T>>>
);


template<typename Encoder>
concept BinaryEncoder = requires(Encoder encoder, const MarketEvent& event) {
    {Encoder::encode(event)} -> ConvertibleToSpan;
};

template<BinaryEncoder Encoder>
class Sequencer;

class Broadcaster;
template<size_t N> class Connection;
class RetransmitServer;

class ExchangeSimulator;
}


#endif