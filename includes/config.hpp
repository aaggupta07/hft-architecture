#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "order.hpp"

#include <cstdint>
#include <cstddef>

namespace config {
	// Order Book (BookState) Configuration
	inline constexpr size_t MAX_PRICE_LEVELS = 1 << 13;
    inline constexpr size_t MAX_CONCURRENT_ORDERS = 1 << 15;

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

	// Each log setting includes all previous log messages as well.
	// Zero performance penalty at "none" (via constexpr) - Set w/ release builds.
	// Minimal performance penalty when set to "errors" or "minimal".
	// Significant performance penalty with "Detailed" or "Extra".
	enum class LogSetting {
		None,			// No log messages. A single message is outputted when each thread stops, with the error.
		Errors,			// Errors only - this includes non-fatal errors like a bad request.
		Minimal,		// Occasional (e.g. 1 in 10000 orders/packets) log messages to quickly, roughly gauge throughput.
						// The market data handler logs any TCP retransmit requests.
		Detailed,		// A single log message at each stage of each component.
		Flush			// Flush every log message to aid in debugging.
	};
}

namespace exchange::config {
	// Logging across the exchange
	inline constexpr ::config::LogSetting LOGGING = ::config::LogSetting::Minimal;

	// Broadcast Configuration
	using Port = uint16_t;
	inline constexpr char	MCAST_GROUP[]	= "239.255.0.1";

	// Use `ifconfig` to find the UDP multicast interface on the local network if 0.0.0.0 does not work
	inline constexpr char	MCAST_INTERFACE[] = "0.0.0.0";
	inline constexpr Port	MCAST_PORT		= 50000;
	
	// Central Limit Order Book Configuration
	inline constexpr size_t MARKET_EVENT_BUFFER_CAPACITY = 1 << 14;

	// Encoded Message Configuration (note: a `MarketEvent` is 18 bytes large when serialized)
	inline constexpr size_t MAX_MESSAGE_BYTES = 1 << 5;

	// Sequencer Configuration
	inline constexpr uint64_t FIRST_SEQUENCE_ID = 1;

	// TCP Retransmit Server Configuration
	inline constexpr size_t 	RETRANSMIT_CACHE_SIZE 		= 1 << 15;
	inline constexpr size_t 	RETRANSMIT_PORT 			= 40000;
	inline constexpr int 		MAX_PENDING_CONNECTIONS 	= 10;
	inline constexpr size_t 	DEFAULT_CONNECTION_BUFFERS 	= 5;
	inline constexpr size_t 	MAX_TOTAL_CONNECTIONS 		= 25;
	// kqueue timeout: Sleeps for up to this much time before checking for a shutdown request
	inline constexpr size_t 	KQUEUE_TIMEOUT_NS 			= 100'000'000;

	// Order Generator Configuration
	inline constexpr double BUY_PROBABILITY 		= 0.3;
	inline constexpr double SELL_PROBABILITY 		= 0.3;
	inline constexpr double CANCEL_PROBABILITY 		= 0.4;

	/* Probability of an order being highly aggressive.
	 * Highly aggressive orders cross the spread with larger price variance and larger quantity.
	 * They are very likely to cross multiple resting orders, and possibly be partially filled. */ 
	inline constexpr double AGGRESSIVE_ORDER_PROBABILITY = 0.02; 
	inline constexpr Order::Quantity MIN_LARGE_QUANTITY = 100;
	inline constexpr Order::Quantity MAX_LARGE_QUANTITY = 600;

	inline constexpr Order::Price MIN_PRICE = 1;
	inline constexpr Order::Price MAX_PRICE = 1000;

	// Used as the midpoint for generating prices when the order book is empty
	inline constexpr Order::Price DEFAULT_BUY_PRICE = 350;
	inline constexpr Order::Price DEFAULT_SELL_PRICE = 650;

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

namespace handler::config {
	// Logging across the market data handler
	inline constexpr ::config::LogSetting LOGGING = ::config::LogSetting::Minimal;

	// Buffer sizes
	inline constexpr size_t UDP_PACKET_BUFFER_SIZE = 1 << 15;
	inline constexpr size_t REORDER_BUFFER_SIZE = 1 << 15;
	inline constexpr size_t MARKET_EVENT_BUFFER_SIZE = 1 << 15;
	inline constexpr size_t RETRANSMIT_BUFFER_SIZE = 1 << 8;

	// Real-Time Feed Listener
	// macOS specifically expects a 4-byte signed int
	inline constexpr int LISTENER_SOCKET_BUFFER_SIZE = 8 * 1024 * 1024; // 8MB

	// TCP Retransmit Client
	inline constexpr const char* SERVER_IP = "127.0.0.1";
}

namespace strategy::config {
	// Logging across the strategy processes
	inline constexpr ::config::LogSetting LOGGING = ::config::LogSetting::Detailed;

	// Buffer sizes
	inline constexpr size_t TRADE_BUFFER_SIZE = 1 << 14;
	

}


#endif
