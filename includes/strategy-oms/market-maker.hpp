#ifndef MARKET_MAKER_HPP
#define MARKET_MAKER_HPP

#include "order.hpp"
#include "protocol.hpp"
#include "strategy.hpp"

#include <optional>

// The strategy assumes that any orders it returns
// will be placed, and must be notified otherwise
namespace strategy {
class PassiveMarketMaker {
private:
	static constexpr size_t MAX_REQUESTS = 4;

	struct PlacedOrder {
		enum class Status {
			Placed,
			NotPlaced,
		};

		Status status = Status::NotPlaced;
		Order placed_order_;

		exchange::OrderRequest to_order_request() const;
	};

	struct PricePair {
		Order::Price bid_price_;
		Order::Price offer_price_;
	};
	
	PlacedOrder buy_order_;
	PlacedOrder sell_order_;

	bool should_place_new_orders(const BestBidOffer& bbo) const;
	std::optional<PricePair> price_new_order_pair(const BestBidOffer& bbo) const;

	size_t get_cancel_orders(std::span<exchange::OrderRequest> order_request_buffer);
	size_t get_new_order_pair(const BestBidOffer& bbo, 
		std::span<exchange::OrderRequest> order_request_buffer);
	
	size_t update_quantity(Order::Side side, const OrderFillUpdate& order_update,
		std::span<exchange::OrderRequest> order_request_buffer);
public:
	PassiveMarketMaker() = default;
	
	size_t get_orders(const BestBidOffer& bbo, std::span<exchange::OrderRequest> order_request_buffer);
	size_t update_placed_order(const OrderFillUpdate& order_update, 
		std::span<exchange::OrderRequest> order_request_buffer);
};
}
#endif