#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "order.hpp"

#include <cstdint>
#include <cstddef>

namespace config {
	// Order Book (BookState) Configuration
	inline constexpr size_t MAX_PRICE_LEVELS = 1 << 10;
    inline constexpr size_t MAX_CONCURRENT_ORDERS = 1 << 14;

	// Set to 128 bytes on Apple Silicon to avoid false sharing on performance cores,
	// and std::hardware_destructive_interference_size on other architecture
	inline constexpr size_t CACHE_LINE_SIZE = 
		#if defined(__APPLE__) || (!defined(__aarch64__) && !defined(__arm64__))
			128
		#else
			#include <new>
			std::hardware_destructive_interference_size
		#endif
	;

}

namespace exchange::config {
	// Broadcast Configuration
	using Port = uint16_t;
	inline constexpr char	MCAST_GROUP[]	= "224.0.1.5";
	inline constexpr Port	MCAST_PORT		= 50000;
	
	// Central Limit Order Book Configuration
	inline constexpr size_t MARKET_EVENT_BUFFER_CAPACITY = 1 << 10;

	// Encoded Message Configuration
	inline constexpr size_t MAX_MESSAGE_BYTES = 1 << 6;

	// Sequencer Configuration (None)

	// TCP Retransmit Server Configuration
	inline constexpr size_t 		RETRANSMIT_CACHE_SIZE 		= 1 << 10;
	inline constexpr const char* 	RETRANSMIT_PORT 			= "40000";
	inline constexpr int 			MAX_PENDING_CONNECTIONS 	= 10;
	inline constexpr bool 			LOGGING						= true;
	inline constexpr size_t 		DEFAULT_CONNECTION_BUFFERS 	= 5;
	inline constexpr size_t 		MAX_TOTAL_CONNECTIONS 		= 25;

	// Order Generator Configuration
	inline constexpr double BUY_PROBABILITY 		= 0.4;
	inline constexpr double CANCEL_PROBABILITY 		= 0.2;
	inline constexpr double SELL_PROBABILITY 		= 0.4;

	/* Probability of an order being highly aggressive.
	 * Highly aggressive orders cross the spread with larger price variance and larger quantity.
	 * They are very likely to cross multiple resting orders, and possibly be partially filled. */ 
	inline constexpr double AGGRESSIVE_ORDER_PROBABILITY = 0.02; 
	inline constexpr Order::Quantity MIN_LARGE_QUANTITY = 400;
	inline constexpr Order::Quantity MAX_LARGE_QUANTITY = 600;

	inline constexpr Order::Price MIN_PRICE = 1;
	inline constexpr Order::Price MAX_PRICE = 1000;

	// Used as the midpoint for generating prices when the order book is empty
	inline constexpr Order::Price DEFAULT_BUY_PRICE = 400;
	inline constexpr Order::Price DEFAULT_SELL_PRICE = 600;

	/* Generated orders uniformly cluster around the best bid and ask prices, 
	 * with a variance of PRICE_VARIANCE on each side of the best price. 
	 * Aggressive orders have a larger variance of LARGE_PRICE_VARIANCE 
	 * on the side of the best opposing order that crosses the spread. */
	inline constexpr size_t PRICE_VARIANCE = 10; // Price = Best Current Price +/- PRICE_VARIANCE
	// Aggressive Price = Best Opposing Price +/- LARGE_PRICE_VARIANCE (on one side only, crossing the spread)
	inline constexpr size_t LARGE_PRICE_VARIANCE = 20; 

	inline constexpr Order::Quantity MIN_QUANTITY = 10;
	inline constexpr Order::Quantity MAX_QUANTITY = 100;

	/* The maximum number of active orders that the generator will track at any given time.
	 * This may be larger than the number of resting orders still on the book, since the generator 
	 * lazily removes filled orders. If reached, the generator will clear a large number of orders 
	 * (either orders that were already filled or by cancelling currently resting orders) 
	 * when it reaches the MAX_ACTIVE_ORDERS limit, so that it can continue generating new orders. */
	inline constexpr size_t MAX_ACTIVE_ORDERS = 1 << 13;
	// When the active order count reaches MAX_ACTIVE_ORDERS, clear until the active order count reaches PURGE_UNTIL_WHEN_FULL
	inline constexpr size_t PURGE_UNTIL_WHEN_FULL = 1 << 12;
	// Number of orders to generate when the active order count reaches 0
	inline constexpr size_t POPULATE_WHEN_EMPTY = 1 << 8; 
}




#endif