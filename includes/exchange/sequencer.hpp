#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include "protocol.hpp"
#include "encoded-message.hpp"
#include "order.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>

namespace exchange {
template<BinaryEncoder Encoder>
class Sequencer {
private:
    SequenceID sequencer_counter_ = 1;
    Encoder encoder_;

public:
    Sequencer(Encoder encoder)
        : encoder_(encoder) {}
    
    EncodedMessage generate_message(const MarketEvent& event);
};

template<BinaryEncoder Encoder>
EncodedMessage Sequencer<Encoder>::generate_message(const MarketEvent& event) {
    auto payload = encoder_.encode(event);
    
    MessageHeader header {
        .sequence_number = sequencer_counter_++,
        .payload_length = payload.size()
    };

    auto message =  EncodedMessage(header, payload);
	message.serialize();
	return message;
}
}

#endif