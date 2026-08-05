#include "market-maker.hpp"
#include "client-id-gen.hpp"

#include <cmath>

namespace strategy {
exchange::OrderRequest PassiveMarketMaker::PlacedOrder::to_order_request() const {
	using exchange::OrderRequest;
	return OrderRequest {
		.order_id = placed_order_.order_id,
		.price = placed_order_.price,
		.quantity = placed_order_.quantity,
		.type = (placed_order_.side == Order::Side::Buy) ? OrderRequest::Type::Buy : OrderRequest::Type::Sell,
	};
}

auto PassiveMarketMaker::price_new_order_pair(const BestBidOffer& bbo) const -> std::optional<PricePair> {
	Order::Price potential_ask_price = bbo.best_offer.price - config::MIN_TICK_DIFF;
	Order::Price potential_buy_price = bbo.best_bid.price + config::MIN_TICK_DIFF;

	if(potential_ask_price - potential_buy_price >= config::ONE_SIDED_MIN_SPREAD * 2) {
		Order::Price new_midpoint_price = (bbo.best_offer.price + bbo.best_bid.price) / 2;

		return PricePair {
			.bid_price_ = std::max(new_midpoint_price - config::ONE_SIDED_MAX_SPREAD, potential_buy_price),
			.offer_price_ = std::min(new_midpoint_price + config::ONE_SIDED_MAX_SPREAD, potential_ask_price),
		};
	}

	return std::nullopt;
}

bool PassiveMarketMaker::should_place_new_orders(const BestBidOffer& bbo) const {
	if(buy_order_.status == PlacedOrder::Status::NotPlaced || sell_order_.status == PlacedOrder::Status::NotPlaced) {
		return true;
	}

	Order::Price new_midpoint_price = (bbo.best_offer.price + bbo.best_bid.price) / 2;
	Order::Price old_midpoint_price = (buy_order_.placed_order_.price + sell_order_.placed_order_.price) / 2;

	return (std::abs(new_midpoint_price - old_midpoint_price) > config::MAX_ALLOWED_DRIFT);
}


// Generate cancel orders (if order is placed), change status to not placed 
size_t PassiveMarketMaker::get_cancel_orders(std::span<exchange::OrderRequest> order_request_buffer) {
	size_t idx = 0;
	if(buy_order_.status == PlacedOrder::Status::Placed) {
		order_request_buffer[idx++] = exchange::OrderRequest {
			.order_id = ClientOrderIDGenerator::generate(),
			.type = exchange::OrderRequest::Type::Cancel,
		};
		buy_order_.status = PlacedOrder::Status::NotPlaced;
	}
	if(sell_order_.status == PlacedOrder::Status::Placed) {
		order_request_buffer[idx++] = exchange::OrderRequest {
			.order_id = ClientOrderIDGenerator::generate(),
			.type = exchange::OrderRequest::Type::Cancel,
		};
		buy_order_.status = PlacedOrder::Status::NotPlaced;
	}
	return idx;
}

// Assert orders are not placed, generate new orders, change status to placed
// Will not place an order if the bid is below a certain threshold
size_t PassiveMarketMaker::get_new_order_pair(const BestBidOffer& bbo, std::span<exchange::OrderRequest> order_request_buffer) {
	constexpr size_t NUM_OF_NEW_ORDERS = 2;
	assert(buy_order_.status == PlacedOrder::Status::NotPlaced && sell_order_.status == PlacedOrder::Status::NotPlaced);
	assert(order_request_buffer.size() >= NUM_OF_NEW_ORDERS);
	auto new_prices = price_new_order_pair(bbo);

	// The spread is too narrow to place a profitable pair of orders
	// or the bid price is too low
	if(!new_prices || new_prices->bid_price_ < config::MIN_BID_PRICE) {
		return 0;
	}

	buy_order_ = {
		.status = PlacedOrder::Status::Placed,
		.placed_order_ = {
			.order_id = ClientOrderIDGenerator::generate(),
			.price = new_prices->bid_price_,
			.quantity = config::MARKET_MAKER_ORDER_QUANTITY,
			.side = Order::Side::Buy,
		},
	};

	sell_order_ = {
		.status = PlacedOrder::Status::Placed,
		.placed_order_ = {
			.order_id = ClientOrderIDGenerator::generate(),
			.price = new_prices->offer_price_,
			.quantity = config::MARKET_MAKER_ORDER_QUANTITY,
			.side = Order::Side::Sell,
		},
	};

	order_request_buffer[0] = buy_order_.to_order_request();
	order_request_buffer[1] = sell_order_.to_order_request();
	return NUM_OF_NEW_ORDERS;
}

size_t PassiveMarketMaker::get_orders(const BestBidOffer& bbo, std::span<exchange::OrderRequest> order_request_buffer) {
	size_t total_orders_placed = 0;
	if(should_place_new_orders(bbo)) {
		total_orders_placed += get_cancel_orders(order_request_buffer.subspan(total_orders_placed));
		total_orders_placed += get_new_order_pair(bbo, order_request_buffer.subspan(total_orders_placed));
	}
	return total_orders_placed;
}

size_t PassiveMarketMaker::update_quantity(Order::Side side, const OrderFillUpdate& order_update,
											std::span<exchange::OrderRequest> order_request_buffer) 
{
	PlacedOrder& order_to_update = (side == Order::Side::Buy) ? buy_order_ : sell_order_;

	if(order_update.filled_quantity >= order_to_update.placed_order_.quantity) {
		order_to_update.status = PlacedOrder::Status::NotPlaced;
		return get_cancel_orders(order_request_buffer);
	}

	order_to_update.placed_order_.quantity -= order_update.filled_quantity;
	return 0;
}

// Reduces the corresponding order by the filled quantity
// If the quantity becomes 0 / below 0 (due to a benign race) the order status is set to NotPlaced
size_t PassiveMarketMaker::update_placed_order(const OrderFillUpdate& order_update, 
							std::span<exchange::OrderRequest> order_request_buffer) 
{
	if(buy_order_.placed_order_.order_id == order_update.order_id) {
		update_quantity(Order::Side::Buy, order_update, order_request_buffer);
		// For now, the passive market maker doesn't manage exposure by updating the opposing side
	}
	else if(sell_order_.placed_order_.order_id == order_update.order_id) {
		update_quantity(Order::Side::Sell, order_update, order_request_buffer);
		// For now, the passive market maker doesn't manage exposure by updating the opposing side
	}
	
	return 0;
}

}