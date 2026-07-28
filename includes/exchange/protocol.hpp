#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <cstddef>
#include <span>
#include "order.hpp"

namespace exchange {

using SequenceID = uint64_t;
using Length = uint16_t;
using Byte = uint8_t;

// A sequence number of 0 is reserved for error messages
// In this case, the payload_length field contains the error code, and the buffer is empty
struct MessageHeader {
    SequenceID sequence_number;
    Length payload_length;

    void serialize(std::span<Byte> buffer) const;
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

class CentralLimitOrderBook;
class MarketRequestGenerator;

class EncodedMessage;
class Broadcaster;
class RetransmitServer;

class ExchangeSimulator;
}


#endif