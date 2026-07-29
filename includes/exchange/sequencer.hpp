#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include "protocol.hpp"
#include "encoded-message.hpp"
#include "order.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdio>
#include <print>

namespace exchange {
template<BinaryEncoder Encoder>
class Sequencer {
private:
    SequenceID sequencer_counter_ = 1;

public:
    Sequencer() = default;
    EncodedMessage generate_message(const MarketEvent& event);
};

template<BinaryEncoder Encoder>
EncodedMessage Sequencer<Encoder>::generate_message(const MarketEvent& event) {
    std::array<std::byte, Encoder::BUFFER_SIZE> payload = Encoder::encode(event);
    
    MessageHeader header {
        .sequence_number = sequencer_counter_++,
        .payload_length = payload.size()
    };

    auto message =  EncodedMessage(header, payload);
	message.serialize();
	if constexpr(config::LOGGING) {
		std::println("[Sequencer] Sequenced packet # {}.", sequencer_counter_ - 1);
		std::fflush(stdout);
	}

	return message;
}
}

#endif
