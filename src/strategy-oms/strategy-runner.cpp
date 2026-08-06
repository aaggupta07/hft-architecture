#include "strategy-runner.hpp"
#include "log.hpp"

#include <thread>

namespace strategy {
StrategyRunner::StrategyRunner(MarketEventBuffer& market_events)
	: order_book_engine_(public_bbo_, market_events),
	oms_(public_risk_data_, trade_buffer_),
	trader_(trade_buffer_, public_bbo_, public_risk_data_, order_update_buffer_) {}

void StrategyRunner::launch_order_book_engine(std::stop_token stop_token) {
	order_book_engine_.start(stop_token);
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>)
		logging::write<config::LOGGING>("StrategyRunner", "Order book engine stopped.");
	request_stop();
}

void StrategyRunner::launch_oms(std::stop_token stop_token) {
	oms_.start(stop_token);
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>)
		logging::write<config::LOGGING>("StrategyRunner", "OMS stopped.");
	request_stop();
}

void StrategyRunner::start(std::stop_token external_stop_token) {
	stop_source_ = std::stop_source {};
	std::stop_callback external_stop_callback(external_stop_token, [this] { request_stop(); });
	const std::stop_token stop_token = stop_source_.get_token();

	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>)
		logging::write<config::LOGGING>("StrategyRunner", "Starting.");
	{
		std::jthread order_book_engine_thread(&StrategyRunner::launch_order_book_engine, this, stop_token);
		std::jthread oms_thread(&StrategyRunner::launch_oms, this, stop_token);
		trader_.start(stop_token);
		request_stop();
	}
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>)
		logging::write<config::LOGGING>("StrategyRunner", "Stopped.");
}
}
