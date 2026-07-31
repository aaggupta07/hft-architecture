#include "market-data-handler.hpp"

#include <cstdio>
#include <print>
#include <thread>

int main() {
	using MarketEventBuffer = LazyRingBuffer<MarketEvent, handler::config::MARKET_EVENT_BUFFER_SIZE>;

	std::println("Starting market-data handler.");
	std::fflush(stdout);

	MarketEventBuffer market_event_buffer;
	handler::MarketDataHandler market_data_handler(market_event_buffer);
	std::jthread event_drainer([&market_event_buffer](std::stop_token stop_token) {
		while(market_event_buffer.wait_get_tail_ref(stop_token) != nullptr) {
			market_event_buffer.consume();
		}
	});
	market_data_handler.start();

	std::println("Market-data handler stopped.");
	std::fflush(stdout);
}
