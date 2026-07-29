#include "exchange-simulator.hpp"
#include <print>
#include <thread>

namespace exchange {
ExchangeSimulator::ExchangeSimulator()
	: clob_(market_event_buffer_),
	order_generator_(clob_),
	retransmit_server_(retransmit_cache_) {}

constexpr void ExchangeSimulator::log(const Error& error) const {
	std::println("[ExchangeSimulator] Error: {}", error);
}

void ExchangeSimulator::launch_retransmit_server() {
	auto result = retransmit_server_.start();
	if(!result && config::LOGGING) log(result.error());
}

void ExchangeSimulator::launch_order_generator() {
	order_generator_.start();
}

void ExchangeSimulator::run() {
	auto broadcast_start_result = real_time_feed_.start();
	if(!broadcast_start_result) [[unlikely]] {
		log(broadcast_start_result.error());
		return;
	}

	while(true) {
		const MarketEvent new_market_event = market_event_buffer_.wait_pop();
		const EncodedMessage new_message = sequencer_.generate_message(new_market_event);
		retransmit_cache_.put_item(new_message);
		auto result = real_time_feed_.send(new_message.message());
		if(!result && config::LOGGING) log(result.error());
	}
}

void ExchangeSimulator::start() {
	std::jthread retransmit_server_thread{&ExchangeSimulator::launch_retransmit_server, this};
	std::jthread order_generator_thread{&ExchangeSimulator::launch_order_generator, this};
	run();
}

}