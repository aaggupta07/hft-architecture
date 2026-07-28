#ifndef ORDER_HPP
#define ORDER_HPP

#include <cstdint>
#include <cstddef>
#include <limits>

struct Order {
	using ID = uint64_t;
	using Price = int32_t;
	using Quantity = uint32_t;
	static constexpr ID INVALID_ORDER_ID = 0;

	enum class Side: uint8_t {
        Buy,
        Sell,
    };

	ID order_id;
    Price price;
  	Quantity quantity;
	Side side;
};

// Order: 32 bytes => Typically, two orders packed onto one cache line
struct RestingOrder {
	using Index = uint32_t;
    static constexpr size_t NULL_INDEX = std::numeric_limits<Index>::max();

	Order order;
    Index previous = NULL_INDEX;
    Index next = NULL_INDEX;
};

// MarketEvent: 32 bytes => Typically, two events packed onto one cache line
struct MarketEvent {
	enum class Type: uint8_t {
		Add,
		Cancel,
		Trade,
	};

	Order order;
	Type type;
};

#endif