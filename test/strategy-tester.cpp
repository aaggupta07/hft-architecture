#include "strategy-runner.hpp"

#include <cstdio>
#include <print>

int main() {
	using MarketEventBuffer = LazyRingBuffer<MarketEvent, handler::config::MARKET_EVENT_BUFFER_SIZE>;

	std::println("Starting strategy runner.");
	std::fflush(stdout);

	MarketEventBuffer market_events;
	strategy::StrategyRunner strategy_runner(market_events);
	strategy_runner.start();

	std::println("Strategy runner stopped.");
	std::fflush(stdout);
}
