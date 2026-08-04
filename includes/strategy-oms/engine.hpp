#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "book-state.hpp"
#include "seq-lock.hpp"
#include "strategy.hpp"

#include <stop_token>

namespace strategy {
class OrderBookEngine {
private:
	BookState state_;
	SeqLock<BestBidOffer>& public_bbo_;
	MarketEventBuffer& market_events_;

public:
	OrderBookEngine(SeqLock<BestBidOffer>& public_bbo, MarketEventBuffer& market_events)
		: public_bbo_(public_bbo), market_events_(market_events) {}

	void start(std::stop_token stop_token);
};
}


#endif