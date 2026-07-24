#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include <arpa/inet.h>
#include <sys/socket.h>

#include <concepts>
#include <span>
#include "protocol.hpp"
#include "encoded-message.hpp"

namespace exchange {
template<typename Encoder>
concept BinaryEncoder = requires(Encoder encoder, const events::ExchangeEvent& event) {
    {encoder.encode(event)} -> std::convertible_to<std::span<uint8_t>>;
};

template<BinaryEncoder Encoder>
class Sequencer {
private:
    SequenceID sequencer_counter_ = 1;
    Encoder encoder_;

public:
    Sequencer(Encoder encoder)
        : encoder_(encoder) {}
    
    EncodedMessage generate_message(const events::ExchangeEvent& event);
};

template<BinaryEncoder Encoder>
EncodedMessage Sequencer<Encoder>::generate_message(const events::ExchangeEvent& event) {
    auto payload = encoder_.encode(event);
    
    MessageHeader header {
        .sequence_number = sequencer_counter_++,
        .payload_length = payload.size()
    };

    return EncodedMessage(header, payload);
}
}

#endif