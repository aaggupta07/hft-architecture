#include "clob.hpp"
#include "order.hpp"

namespace exchange {
	std::expected<std::vector<MarketEvent>, Error> CentralLimitOrderBook::submit(const OrderRequest& request) {
		switch(request.type) {
			case OrderRequest::Type::Buy:
				return buy_order(request);
			case OrderRequest::Type::Sell:
				return sell_order(request);
			case OrderRequest::Type::Cancel:
				return cancel_order(request);
		}
		return {};
	}


	std::optional<Order::ID> CentralLimitOrderBook::sample_resting_order() const {
		// TODO: Implement a method to sample a resting order from the order book
		/*
			Create a static random device to sample a resting order from the order book.
			- If the order book is empty, return std::nullopt.
			- If the order book has orders, randomly select one and return its ID.
		*/
		return std::nullopt;
	}

	std::expected<std::vector<MarketEvent>, Error> CentralLimitOrderBook::buy_order(const OrderRequest& request) {
		// TODO: Implement the logic for processing a buy order
		/*
			Compare price to the BBO and determine if it can be matched with existing sell orders.
			- If it can be matched, execute the trade(s) repeatedly and return the executed orders.
			- If it cannot be matched, add the order to the order book and return an ADD order.
		*/
		return {};
	}

	std::expected<std::vector<MarketEvent>, Error> CentralLimitOrderBook::sell_order(const OrderRequest& request) {
		// TODO: Implement the logic for processing a sell order
		/*
			Compare price to the BBO and determine if it can be matched with existing buy orders.
			- If it can be matched, execute the trade(s) repeatedly and return the executed orders.
			- If it cannot be matched, add the order to the order book and return an ADD order.
		*/
		return {};
	}

	std::expected<std::vector<MarketEvent>, Error> CentralLimitOrderBook::cancel_order(const OrderRequest& request) {
		if(state_.order_exists(request.order_id)) {
			state_.cancel(request.order_id);

			Order new_order { .order_id = request.order_id };
			MarketEvent canceled_order {
				.order = new_order,
				.type = MarketEvent::Type::Cancel,
			};
			
			return std::vector<MarketEvent>{canceled_order};
		}
		
		else {
			return std::unexpected(Error::OrderDoesNotExist);
		}

		return {};
	}
}