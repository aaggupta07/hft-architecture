#ifndef BOE_FORMAT_HPP
#define BOE_FORMAT_HPP

#include "order.hpp"
#include <span>
#include <array>

namespace exchange{
class BinaryOrderExchangeFormat {
public:
	static constexpr size_t BUFFER_SIZE = sizeof(MarketEvent);

	BinaryOrderExchangeFormat() = delete;
	BinaryOrderExchangeFormat(const BinaryOrderExchangeFormat&) = delete;
	BinaryOrderExchangeFormat& operator=(const BinaryOrderExchangeFormat&) = delete;
	BinaryOrderExchangeFormat(BinaryOrderExchangeFormat&&) = delete;
	BinaryOrderExchangeFormat& operator=(BinaryOrderExchangeFormat&&) = delete;

	static std::array<std::byte, sizeof(MarketEvent)> encode(const MarketEvent& event);
	static MarketEvent decode(std::span<const std::byte> buffer);
};
}

#endif