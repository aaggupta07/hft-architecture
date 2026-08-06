#ifndef MARKET_MAKER_HPP
#define MARKET_MAKER_HPP

#include "order.hpp"
#include "strategy.hpp"

#include <optional>

namespace strategy {
class PassiveMarketMaker {
private:
	struct PlacedOrder {
		enum class Status : uint8_t { NotPlaced, Placed };

		Status status = Status::NotPlaced;
		Order placed_order{};

		OutboundOrderRequest new_order_request() const noexcept;
	};

	struct PricePair {
		Order::Price bid_price;
		Order::Price offer_price;
	};

	PlacedOrder buy_order_;
	PlacedOrder sell_order_;
	int64_t net_filled_quantity_ = 0;

	bool should_place_new_orders(const BestBidOffer& bbo) const;
	std::optional<PricePair> price_new_order_pair(const BestBidOffer& bbo) const;

	bool queue_cancel_orders(StrategyOrderBuffer& order_buffer);
	bool queue_new_order_pair(const BestBidOffer& bbo, StrategyOrderBuffer& order_buffer);
	bool queue_quantity_update(PlacedOrder& order, Order::Quantity quantity, StrategyOrderBuffer& order_buffer);

	void rebalance_exposure(StrategyOrderBuffer& order_buffer);
	void cancel_for_risk(const OutboundOrderRequest& rejected, StrategyOrderBuffer& order_buffer);

public:
	PassiveMarketMaker() = default;

	void get_orders		(const BestBidOffer& bbo, StrategyOrderBuffer& order_buffer);
	void update			(const OrderFillUpdate& order_update, StrategyOrderBuffer& order_buffer);
	void notify_invalid	(const OutboundOrderRequest& rejected, OrderStatus reason, StrategyOrderBuffer& order_buffer);
};
}

#endif
