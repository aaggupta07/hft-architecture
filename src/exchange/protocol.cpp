#include "protocol.hpp"
#include <cassert>
#include <cstring>

namespace exchange {
void MessageHeader::serialize(std::span<std::byte> buffer) const {
    assert(buffer.size() >= PACKED_WIRE_SIZE);
    
    SequenceID network_sequence_number = htonll(sequence_number);
    Length network_payload_length = htons(payload_length);
    std::memcpy(buffer.data(), &network_sequence_number, sizeof(network_sequence_number));
    std::memcpy(buffer.data() + sizeof(network_sequence_number), &network_payload_length, sizeof(network_payload_length));
}

}
