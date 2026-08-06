#ifndef STRATEGY_HPP
#define STRATEGY_HPP

#include "book-state.hpp"
#include "protocol.hpp"
#include "lazy-ring-buffer.hpp"
#include "single-thread-ring-buffer.hpp"

#include <concepts>

namespace strategy {
struct OrderFillUpdate {
	Order::ID order_id;
	Order::Quantity filled_quantity;
};

using BestBidOffer = BookState::OrderSnapshot;
using MarketEventBuffer = LazyRingBuffer<MarketEvent, handler::config::MARKET_EVENT_BUFFER_SIZE>;
using TradeBuffer = LazyRingBuffer<exchange::OrderRequest, config::TRADE_BUFFER_SIZE>;
using OrderFillUpdateBuffer = LazyRingBuffer<OrderFillUpdate, config::ORDER_UPDATE_BUFFER_SIZE>;

enum class QuantityChange : uint8_t {
	NotAnUpdate,
	Reducing,
	Increasing,
};

// Metadata lets risk distinguish an UPDATE that adds exposure from one that
// removes it. previous_quantity restores local strategy state after rejection.
struct OutboundOrderRequest {
	exchange::OrderRequest request{};
	QuantityChange quantity_change = QuantityChange::NotAnUpdate;
	Order::Quantity previous_quantity = 0;
	Order::Side side = Order::Side::Buy;
};

using StrategyOrderBuffer = SingleThreadRingBuffer<OutboundOrderRequest, config::STRATEGY_ORDER_BUFFER_SIZE>;

struct RiskData {
	using Position 		= int64_t;
	using PnL 			= int64_t;
	using OrderCount 	= uint32_t;

	Position 	net_position;
	PnL 		realized_pnl;			// Fixed point
	PnL		 	unrealized_pnl;			// Fixed point
	OrderCount 	open_order_count;
	OrderCount 	orders_sent_in_window;

	Position 	max_position;			// Cannot be more than +/- max_position
	PnL 		max_loss_allowed;
	OrderCount 	max_orders_allowed;
	OrderCount 	max_orders_per_window;

	bool 		is_halted;
};

enum class OrderStatus: uint8_t {
	Good,
	Stopped, 			// via stop_token
	Halted,				// via OMS halt
	TooManyOrders,
	TooMuchExposure,
	TooMuchLoss,
};

class OrderBookEngine;

class PassiveMarketMaker;

template<typename T>
concept Strategy = requires(T strategy, const BestBidOffer& bbo, const OrderFillUpdate& update,
	const OutboundOrderRequest& request, OrderStatus reason, StrategyOrderBuffer& order_buffer) {
	{strategy.get_orders(bbo, order_buffer)} -> std::same_as<void>;
	{strategy.update(update, order_buffer)} -> std::same_as<void>;
	{strategy.notify_invalid(request, reason, order_buffer)} -> std::same_as<void>;
};

template<typename T> requires Strategy<T>
class Trader;

class OrderManagementSystem;

class ClientOrderIDGenerator;
}

#endif
