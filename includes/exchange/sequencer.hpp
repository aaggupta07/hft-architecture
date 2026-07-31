#ifndef SEQUENCER_HPP
#define SEQUENCER_HPP

#include "protocol.hpp"
#include "encoded-message.hpp"
#include "order.hpp"
#include "config.hpp"
#include "log.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>

namespace exchange {
template<BinaryEncoder Encoder>
class Sequencer {
private:
    SequenceID sequencer_counter_ = config::FIRST_SEQUENCE_ID;

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
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) {
		logging::write<config::LOGGING>("Sequencer", "Sequenced packet # {}.", sequencer_counter_ - 1);
	}
	else if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) {
		if((sequencer_counter_ - 1) % logging::MINIMAL_INTERVAL == 0) logging::write<config::LOGGING>("Sequencer", "Sequenced packet # {}.", sequencer_counter_ - 1);
	}

	return message;
}
}

#endif
