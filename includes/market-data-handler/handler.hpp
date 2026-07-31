#ifndef HANDLER_HPP
#define HANDLER_HPP

#include <cstdint>
#include <cstddef>
#include <concepts>
#include <span>
#include <expected>

#include "order.hpp"
#include "binary-protocol.hpp"
#include "reorder-buffer.hpp"
#include "tcp-retransmit.hpp"
#include "config.hpp"
#include "ring-buffer.hpp"
#include "lazy-ring-buffer.hpp"
#include "handler-errors.hpp"


namespace handler {
using SequenceID = uint64_t;
using SerializedMarketEvent = std::array<std::byte, exchange::BinaryProtocol::BUFFER_SIZE>;
using SerializedMarketEventSpan = const std::span<const std::byte>;
using RetransmitRequestBuffer = SharedRingBuffer<exchange::RetransmitRequest, config::RETRANSMIT_BUFFER_SIZE>;
using MarketReorderBuffer = ReorderBuffer<SerializedMarketEvent, config::REORDER_BUFFER_SIZE>;
using PacketBuffer = LazyRingBuffer<exchange::EncodedMessage, config::UDP_PACKET_BUFFER_SIZE>;

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

std::expected<void, Error> write_market_data_to_reorder_buffer(
	MarketReorderBuffer& reorder_buffer, SerializedMarketEventSpan event, SequenceID packet_id);
};

#endif