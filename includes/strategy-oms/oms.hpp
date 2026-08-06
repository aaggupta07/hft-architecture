#ifndef OMS_HPP
#define OMS_HPP

#include "seq-lock.hpp"
#include "strategy.hpp"
#include "book-state.hpp"
#include "protocol.hpp"

#include <stop_token>


namespace strategy {

class OrderManagementSystem {
private:
	SeqLock<RiskData>& public_risk_data_;
	TradeBuffer& trade_buffer_;
	
	bool compute_risk_adjustments(const BookState::OrderSnapshot& order_snapshot,
		const exchange::OrderRequest& new_order);

public:
	OrderManagementSystem(SeqLock<RiskData>& public_risk_data, TradeBuffer& trade_buffer)
		: public_risk_data_(public_risk_data), trade_buffer_(trade_buffer) {}
	
	void start(std::stop_token stop_token); 
};
}

#endif
