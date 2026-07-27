#ifndef ORDER_HPP
#define ORDER_HPP

#include <cstdint>
#include <cstddef>
#include <limits>

// Order: 32 bytes => Typically, two orders packed onto one cache line
struct Order {
	using ID = uint64_t;
	using Price = int32_t;
	using Quantity = uint32_t;
	using Index = uint32_t;

    enum class Side: bool {
        Buy,
        Sell,
    };

	enum class Type: short {
		Add,
		Cancel,
		Trade,
	};
    
    static constexpr size_t NULL_INDEX = std::numeric_limits<Index>::max();
    ID order_id;
    Price price;
    Quantity quantity;
    Index previous = NULL_INDEX;
    Index next = NULL_INDEX;
    Side side;
	Type type;
};

#endif