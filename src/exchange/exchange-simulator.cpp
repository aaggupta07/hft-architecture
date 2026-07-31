#include "exchange-simulator.hpp"
#include "log.hpp"
#include <thread>

namespace exchange {
ExchangeSimulator::ExchangeSimulator()
	: clob_(market_event_buffer_),
	order_generator_(clob_),
	retransmit_server_(retransmit_cache_) {}

void ExchangeSimulator::launch_retransmit_server(std::stop_token stop_token) {
	auto result = retransmit_server_.start(stop_token);
	if(!result) [[unlikely]] { if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("ExchangeSimulator", "Error: {}", result.error()); }
	else if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("ExchangeSimulator", "Retransmit server stopped.");
	request_stop();
}

void ExchangeSimulator::launch_order_generator(std::stop_token stop_token) {
	order_generator_.start(stop_token);
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("ExchangeSimulator", "Order generator stopped.");
	request_stop();
}

void ExchangeSimulator::run(std::stop_token stop_token) {
	auto broadcast_start_result = real_time_feed_.start();
	if(!broadcast_start_result) [[unlikely]] {
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("ExchangeSimulator", "Error: {}", broadcast_start_result.error());
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("ExchangeSimulator", "Stopping because the broadcaster could not start.");
		return;
	}

	while(!stop_token.stop_requested()) {
		MarketEvent* new_market_event = market_event_buffer_.wait_get_tail_ref(stop_token);
		if(!new_market_event) {
			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("ExchangeSimulator", "Stopped while waiting for a market event.");
			break;
		}
		const EncodedMessage new_message = sequencer_.generate_message(*new_market_event);
		market_event_buffer_.consume();

		retransmit_cache_.put_item(new_message);
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) {
			logging::write<config::LOGGING>("ExchangeSimulator", "Cached packet # {}.", new_message.header().sequence_number);
		}
		else if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) {
			if(new_message.header().sequence_number % logging::MINIMAL_INTERVAL == 0) logging::write<config::LOGGING>("ExchangeSimulator", "Cached packet # {}.", new_message.header().sequence_number);
		}
		auto result = real_time_feed_.send(new_message.message());
		if(!result) [[unlikely]] if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("ExchangeSimulator", "Error: {}", result.error());
	}

	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("ExchangeSimulator", "Market-event loop {}.", stop_token.stop_requested() ? "stopped" : "exited");
}

void ExchangeSimulator::start(std::stop_token external_stop_token) {
	stop_source_ = std::stop_source {};
	std::stop_callback external_stop_callback {external_stop_token, [this] { request_stop(); }};
	const std::stop_token stop_token = stop_source_.get_token();

	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("ExchangeSimulator", "Starting.");
	{
		std::jthread retransmit_server_thread{&ExchangeSimulator::launch_retransmit_server, this, stop_token};
		std::jthread order_generator_thread{&ExchangeSimulator::launch_order_generator, this, stop_token};
		run(stop_token);
		request_stop();
	}
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("ExchangeSimulator", "Stopped.");
}

}
