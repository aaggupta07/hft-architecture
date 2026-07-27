#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <cstdint>
#include <cstddef>
#include <span>

namespace exchange {

using SequenceID = uint64_t;
using Length = uint16_t;
using Byte = uint8_t;
constexpr size_t MAX_MESSAGE_BYTES = 1 << 10;

// A sequence number of 0 is reserved for error messages
// In this case, the payload_length field contains the error code, and the buffer is empty
struct MessageHeader {
    SequenceID sequence_number;
    Length payload_length;

    void serialize(std::span<Byte> buffer) const;
};

class CentralLimitOrderBook;
class MarketRequestGenerator;

class EncodedMessage;
class Broadcaster;
class RetransmitServer;

class ExchangeSimulator;
}


#endif