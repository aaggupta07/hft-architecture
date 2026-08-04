#ifndef TRADER_HPP
#define TRADER_HPP

#include "strategy.hpp"
#include "seq-lock.hpp"

#include <array>

namespace strategy {
template<typename T, size_t N>
requires Strategy<T, N>
class Trader {
private:
	std::array<exchange::OrderRequest, N> order_buffer_;
	TradeBuffer& trade_buffer_;
	const SeqLockReader<RiskData>& risk_data_;

	std::stop_source stop_source;

	void send_to_exchange() const { /* Placeholder for now */ }

public:
	Trader(TradeBuffer& trade_buffer, const SeqLockReader<RiskData>& risk_data)
		: trade_buffer_(trade_buffer), risk_data_(risk_data) {}

	void start(std::stop_token external_token = {});
};

}

#endif