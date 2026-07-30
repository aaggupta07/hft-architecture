#ifndef HANDLER_HPP
#define HANDLER_HPP

#include <cstdint>
#include <cstddef>
#include <concepts>
#include <span>

#include "order.hpp"
#include "binary-protocol.hpp"

namespace handler {
using SequenceID = uint64_t;
using SerializedMarketEvent = std::array<std::byte, exchange::BinaryProtocol::BUFFER_SIZE>;

class MulticastListener;
class GapDetector;
class RetransmitClient;

template<typename Decoder>
concept BinaryDecoder = requires(Decoder decoder, const std::span<const std::byte> serialized_data) {
    {Decoder::decode(serialized_data)} -> std::convertible_to<MarketEvent>;
};

template<BinaryDecoder Decoder>
class MarketEventPublisher;

class MarketDataHandler;

};

#endif