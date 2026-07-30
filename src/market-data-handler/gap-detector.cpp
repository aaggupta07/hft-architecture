#include "gap-detector.hpp"

namespace handler {
std::optional<exchange::RetransmitRequest> GapDetector::has_gap(const exchange::MessageHeader& new_packet_header) {
	SequenceID new_sequence_id = new_packet_header.sequence_number;
	if(new_sequence_id > most_recent_packet_id + 1) {
		exchange::RetransmitRequest new_request {
			.first_packet = most_recent_packet_id + 1,
		};
		new_request.last_packet = most_recent_packet_id = new_sequence_id;
		return new_request;
	}
	if(new_sequence_id == most_recent_packet_id + 1) ++new_sequence_id;
	return std::nullopt;
}
}