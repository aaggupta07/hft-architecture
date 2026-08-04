#include "engine.hpp"

namespace strategy {
void OrderBookEngine::start(std::stop_token stop_token) {
	while(!stop_token.stop_requested()) {
		MarketEvent* new_market_event = market_events_.wait_get_tail_ref(stop_token);
		if(new_market_event == nullptr) [[unlikely]] return;
		BookState::Update status = state_.execute(*new_market_event);

		if(status == BookState::Update::NewBestBid || status == BookState::Update::NewBestOffer) {
			public_bbo_.write(state_.snapshot());
		}

		market_events_.consume();
	}
}

}