#ifndef GAP_DETECTOR_HPP
#define GAP_DETECTOR_HPP

#include "handler.hpp"
#include "config.hpp"
#include "tcp-retransmit.hpp"

#include <optional>

namespace handler {
class GapDetector {
private:
	SequenceID most_recent_packet_id;
public:
	GapDetector(SequenceID first_packet_id = exchange::config::FIRST_SEQUENCE_ID)
		: most_recent_packet_id(first_packet_id) {}

	std::optional<exchange::RetransmitRequest> has_gap(const exchange::MessageHeader& new_packet_header);
};
}

#endif