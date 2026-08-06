#ifndef STRATEGY_RUNNER_HPP
#define STRATEGY_RUNNER_HPP

#include "engine.hpp"
#include "market-maker.hpp"
#include "oms.hpp"
#include "trader.hpp"

#include <stop_token>

namespace strategy {
class StrategyRunner {
private:
	SeqLock<BestBidOffer> public_bbo_;
	SeqLock<RiskData> public_risk_data_;
	TradeBuffer trade_buffer_;
	OrderFillUpdateBuffer order_update_buffer_;

	OrderBookEngine order_book_engine_;
	OrderManagementSystem oms_;
	Trader<PassiveMarketMaker> trader_;

	std::stop_source stop_source_;

	void launch_order_book_engine(std::stop_token stop_token);
	void launch_oms(std::stop_token stop_token);

public:
	explicit StrategyRunner(MarketEventBuffer& market_events);
	~StrategyRunner() = default;

	StrategyRunner(const StrategyRunner&) = delete;
	StrategyRunner& operator=(const StrategyRunner&) = delete;
	StrategyRunner(StrategyRunner&&) = delete;
	StrategyRunner& operator=(StrategyRunner&&) = delete;

	void start(std::stop_token external_stop_token = {});
	void request_stop() noexcept { stop_source_.request_stop(); }
};
}

#endif
