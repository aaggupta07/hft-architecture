#include "market-maker.hpp"
#include "client-id-gen.hpp"
#include "log.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace strategy {
OutboundOrderRequest PassiveMarketMaker::PlacedOrder::new_order_request() const noexcept {
	return {
		.request = {
			.order_id = placed_order.order_id,
			.price = placed_order.price,
			.quantity = placed_order.quantity,
			.type = placed_order.side == Order::Side::Buy
				? exchange::OrderRequest::Type::Buy : exchange::OrderRequest::Type::Sell,
		},
		.side = placed_order.side,
	};
}

auto PassiveMarketMaker::price_new_order_pair(const BestBidOffer& bbo) const -> std::optional<PricePair> {
	if(bbo.best_bid.price == BookState::NO_BID || bbo.best_offer.price == BookState::NO_OFFER ||
		bbo.best_offer.price <= bbo.best_bid.price) return std::nullopt;

	const Order::Price potential_offer = bbo.best_offer.price - config::MIN_TICK_DIFF;
	const Order::Price potential_bid = bbo.best_bid.price + config::MIN_TICK_DIFF;
	if(potential_offer - potential_bid < config::ONE_SIDED_MIN_SPREAD * 2) return std::nullopt;

	const Order::Price midpoint = (bbo.best_offer.price + bbo.best_bid.price) / 2;
	return PricePair{
		.bid_price = std::max(midpoint - config::ONE_SIDED_MAX_SPREAD, potential_bid),
		.offer_price = std::min(midpoint + config::ONE_SIDED_MAX_SPREAD, potential_offer),
	};
}

bool PassiveMarketMaker::should_place_new_orders(const BestBidOffer& bbo) const {
	if(buy_order_.status == PlacedOrder::Status::NotPlaced || sell_order_.status == PlacedOrder::Status::NotPlaced) return true;
	const Order::Price midpoint = (bbo.best_offer.price + bbo.best_bid.price) / 2;
	const Order::Price quoted_midpoint = (buy_order_.placed_order.price + sell_order_.placed_order.price) / 2;
	return std::abs(midpoint - quoted_midpoint) > config::MAX_ALLOWED_DRIFT;
}

bool PassiveMarketMaker::queue_cancel_orders(StrategyOrderBuffer& order_buffer) {
	const size_t count = (buy_order_.status == PlacedOrder::Status::Placed) +
		(sell_order_.status == PlacedOrder::Status::Placed);
	if(order_buffer.available() < count) return false;

	auto queue_cancel = [&order_buffer](PlacedOrder& order) {
		if(order.status != PlacedOrder::Status::Placed) return;
		const bool queued = order_buffer.try_push({
			.request = {.order_id = order.placed_order.order_id, .type = exchange::OrderRequest::Type::Cancel},
			.side = order.placed_order.side,
		});
		assert(queued);
		order.status = PlacedOrder::Status::NotPlaced;
	};
	queue_cancel(buy_order_);
	queue_cancel(sell_order_);
	return true;
}

bool PassiveMarketMaker::queue_new_order_pair(const BestBidOffer& bbo, StrategyOrderBuffer& order_buffer) {
	assert(buy_order_.status == PlacedOrder::Status::NotPlaced && sell_order_.status == PlacedOrder::Status::NotPlaced);
	const auto prices = price_new_order_pair(bbo);
	if(!prices || prices->bid_price < config::MIN_BID_PRICE) return true;
	if(order_buffer.available() < 2) return false;

	buy_order_ = {
		.status = PlacedOrder::Status::Placed,
		.placed_order = {
			.order_id = ClientOrderIDGenerator::generate(),
			.price = prices->bid_price,
			.quantity = config::MARKET_MAKER_ORDER_QUANTITY,
			.side = Order::Side::Buy,
		},
	};
	sell_order_ = {
		.status = PlacedOrder::Status::Placed,
		.placed_order = {
			.order_id = ClientOrderIDGenerator::generate(),
			.price = prices->offer_price,
			.quantity = config::MARKET_MAKER_ORDER_QUANTITY,
			.side = Order::Side::Sell,
		},
	};

	const bool bid_queued = order_buffer.try_push(buy_order_.new_order_request());
	const bool offer_queued = order_buffer.try_push(sell_order_.new_order_request());
	assert(bid_queued && offer_queued);
	return true;
}

bool PassiveMarketMaker::queue_quantity_update(PlacedOrder& order, Order::Quantity quantity,
	StrategyOrderBuffer& order_buffer) {
	if(order.status != PlacedOrder::Status::Placed || order.placed_order.quantity == quantity) return true;
	const Order::Quantity previous = order.placed_order.quantity;
	const QuantityChange change = quantity > previous ? QuantityChange::Increasing : QuantityChange::Reducing;
	if(!order_buffer.try_push({
		.request = {
			.order_id = order.placed_order.order_id,
			.quantity = quantity,
			.type = exchange::OrderRequest::Type::Update,
		},
		.quantity_change = change,
		.previous_quantity = previous,
		.side = order.placed_order.side,
	})) return false;

	order.placed_order.quantity = quantity;
	return true;
}

void PassiveMarketMaker::rebalance_exposure(StrategyOrderBuffer& order_buffer) {
	const int64_t threshold = config::EXPOSURE_REBALANCE_THRESHOLD;
	if(std::abs(net_filled_quantity_) < threshold) return;
	const auto imbalance = static_cast<Order::Quantity>(std::min<int64_t>(
		std::abs(net_filled_quantity_), config::MAX_ADDITIONAL_EXIT_QUANTITY));
	const auto base = config::MARKET_MAKER_ORDER_QUANTITY;
	const auto minimum = config::MIN_MARKET_MAKER_ORDER_QUANTITY;
	if(net_filled_quantity_ > 0) {
		const auto bid = std::max(minimum, base > imbalance ? base - imbalance : 0U);
		const auto offer = base + imbalance;
		queue_quantity_update(buy_order_, bid, order_buffer);
		queue_quantity_update(sell_order_, offer, order_buffer);
	} else {
		const auto bid = base + imbalance;
		const auto offer = std::max(minimum, base > imbalance ? base - imbalance : 0U);
		queue_quantity_update(buy_order_, bid, order_buffer);
		queue_quantity_update(sell_order_, offer, order_buffer);
	}
}

void PassiveMarketMaker::get_orders(const BestBidOffer& bbo, StrategyOrderBuffer& order_buffer) {
	if(!should_place_new_orders(bbo)) return;
	const size_t cancels = (buy_order_.status == PlacedOrder::Status::Placed) +
		(sell_order_.status == PlacedOrder::Status::Placed);
	if(order_buffer.available() < cancels + 2) return;
	if(!queue_cancel_orders(order_buffer)) return;
	if(!queue_new_order_pair(bbo, order_buffer)) [[unlikely]] {
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>)
			logging::write<config::LOGGING>("PassiveMarketMaker", "Could not queue a replacement quote pair.");
	}
}

void PassiveMarketMaker::update(const OrderFillUpdate& update, StrategyOrderBuffer& order_buffer) {
	PlacedOrder* order = nullptr;
	if(buy_order_.status == PlacedOrder::Status::Placed && buy_order_.placed_order.order_id == update.order_id) order = &buy_order_;
	else if(sell_order_.status == PlacedOrder::Status::Placed && sell_order_.placed_order.order_id == update.order_id) order = &sell_order_;
	else return;

	const Order::Quantity filled = std::min(update.filled_quantity, order->placed_order.quantity);
	if(order->placed_order.side == Order::Side::Buy) net_filled_quantity_ += filled;
	else net_filled_quantity_ -= filled;
	order->placed_order.quantity -= filled;
	if(order->placed_order.quantity == 0) order->status = PlacedOrder::Status::NotPlaced;
	rebalance_exposure(order_buffer);
}

void PassiveMarketMaker::cancel_for_risk(const OutboundOrderRequest& rejected, StrategyOrderBuffer& order_buffer) {
	PlacedOrder* order = rejected.side == Order::Side::Buy ? &buy_order_ : &sell_order_;
	if(order->status != PlacedOrder::Status::Placed) return;
	if(order_buffer.try_push({.request = {.order_id = order->placed_order.order_id,
		.type = exchange::OrderRequest::Type::Cancel}, .side = order->placed_order.side})) {
		order->status = PlacedOrder::Status::NotPlaced;
	}
}

void PassiveMarketMaker::notify_invalid(const OutboundOrderRequest& rejected, OrderStatus reason,
	StrategyOrderBuffer& order_buffer) {
	if(rejected.request.type == exchange::OrderRequest::Type::Update) {
		PlacedOrder* order = rejected.request.order_id == buy_order_.placed_order.order_id ? &buy_order_ :
			rejected.request.order_id == sell_order_.placed_order.order_id ? &sell_order_ : nullptr;
		if(order != nullptr) order->placed_order.quantity = rejected.previous_quantity;
	} else if(rejected.request.type == exchange::OrderRequest::Type::Buy &&
		buy_order_.placed_order.order_id == rejected.request.order_id) {
		buy_order_.status = PlacedOrder::Status::NotPlaced;
	} else if(rejected.request.type == exchange::OrderRequest::Type::Sell &&
		sell_order_.placed_order.order_id == rejected.request.order_id) {
		sell_order_.status = PlacedOrder::Status::NotPlaced;
	}

	if(reason == OrderStatus::Halted || reason == OrderStatus::TooMuchLoss) {
		(void)queue_cancel_orders(order_buffer);
	} else if(reason == OrderStatus::TooMuchExposure) {
		cancel_for_risk(rejected, order_buffer);
	}
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>)
		logging::write<config::LOGGING>("PassiveMarketMaker", "Rejected order #{}: {}.", rejected.request.order_id,
			static_cast<unsigned>(reason));
}
}
