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

template<typename Encoder>
concept BinaryEncoder = requires(Encoder encoder, const MarketEvent& event) {
	{Encoder::BUFFER_SIZE} -> std::convertible_to<size_t>;
	typename std::integral_constant<std::size_t, Encoder::BUFFER_SIZE>;
    {Encoder::encode(event)} -> std::convertible_to<std::array<std::byte, Encoder::BUFFER_SIZE>>;
};

template<BinaryEncoder Encoder>
class Sequencer;

class Broadcaster;
template<size_t N> class Connection;
class RetransmitServer;

class ExchangeSimulator;
}


#endif