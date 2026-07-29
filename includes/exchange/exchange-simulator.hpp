#ifndef EXCHANGE_SIMULATOR_HPP
#define EXCHANGE_SIMULATOR_HPP

#include "protocol.hpp"
#include "exchange-errors.hpp"

#include "order-generator.hpp"
#include "clob.hpp"
#include "ring-buffer.hpp"

#include "boe-format.hpp"
#include "sequencer.hpp"

#include "circular-cache.hpp"
#include "broadcaster.hpp"
#include "tcp-retransmit.hpp"

#include <stop_token>
#include <string_view>


class exchange::ExchangeSimulator {
private:
	constexpr void log(const Error& error) const;
	constexpr void log(std::string_view message) const;
	void launch_retransmit_server(std::stop_token stop_token);
	void launch_order_generator(std::stop_token stop_token);

	void run(std::stop_token stop_token);

	using MarketEventBuffer = SharedRingBuffer<MarketEvent, config::MARKET_EVENT_BUFFER_CAPACITY>;
	
	MarketEventBuffer market_event_buffer_;
	CentralLimitOrderBook clob_;
	MarketRequestGenerator order_generator_;

	Sequencer<BinaryOrderExchangeFormat> sequencer_;

	using RetransmitCache = CircularCache<EncodedMessage, config::RETRANSMIT_CACHE_SIZE>;
	RetransmitCache retransmit_cache_;
	RetransmitServer retransmit_server_;
	Broadcaster real_time_feed_;
	std::stop_source stop_source_;
	
public:
	ExchangeSimulator();
	~ExchangeSimulator() = default;

	ExchangeSimulator(const ExchangeSimulator&) = delete;
	ExchangeSimulator& operator=(const ExchangeSimulator&) = delete;
	ExchangeSimulator(ExchangeSimulator&&) = delete;
	ExchangeSimulator& operator=(ExchangeSimulator&&) = delete;

	void start(std::stop_token stop_token = {});
	void request_stop() noexcept { stop_source_.request_stop(); }
};

#endif
