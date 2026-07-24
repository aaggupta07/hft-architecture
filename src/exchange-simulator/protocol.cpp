#include "protocol.hpp"
#include <cassert>

namespace exchange {
void MessageHeader::serialize(std::span<Byte> buffer) const {
    assert(buffer.size() >= sizeof(MessageHeader));
    
    SequenceID network_sequence_number = ntohll(sequence_number);
    Length network_payload_length = ntohs(payload_length);
    std::memcpy(buffer.data(), &network_sequence_number, sizeof(sequence_number));
    std::memcpy(buffer.data() + sizeof(network_payload_length), &network_payload_length, sizeof(payload_length));
} 

}