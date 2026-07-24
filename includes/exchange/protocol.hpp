#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <cstddef>
#include <variant>
#include <span>

namespace exchange {

using SequenceID = uint64_t;
using Length = uint16_t;
using Byte = uint8_t;
constexpr size_t MAX_MESSAGE_BYTES = 1 << 10;

struct MessageHeader {
    SequenceID sequence_number;
    Length payload_length;

    void serialize(std::span<Byte> buffer) const;
};

class EncodedMessage;
class Broadcaster;
class RetransmitServer;

class ExchangeSimulator;

namespace events {
class EventGenerator;

using OrderID = uint64_t;
using Price = int32_t;
using Quantity = uint32_t;
using Side = char;

struct AddOrderEvent {
    OrderID order_id;
    Price price;
    Quantity quantity;
    Side side;
};

struct CancelOrderEvent {
    OrderID order_id;
};

struct TradeOrderEvent {
    OrderID order_id;
    Quantity quantity;
};

using ExchangeEvent = std::variant<AddOrderEvent, CancelOrderEvent, TradeOrderEvent>;
}

}


#endif