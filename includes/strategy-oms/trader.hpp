#ifndef TRADER_HPP
#define TRADER_HPP

#include "strategy.hpp"
#include "seq-lock.hpp"
#include "strategy-errors.hpp"
#include "log.hpp"

#include <cmath>
#include <expected>
#include <optional>
#include <stop_token>

namespace strategy {
template<typename T> requires Strategy<T>
class Trader {
private:
	StrategyOrderBuffer order_buffer_;
	TradeBuffer& trade_buffer_;
	const SeqLockReader<BestBidOffer> bbo_;
	const SeqLockReader<RiskData> risk_data_;
	OrderFillUpdateBuffer& update_buffer_;
	T strategy_;

	std::expected<void, Error> place_new_orders(std::stop_token stop_token);
	void send_to_exchange(const exchange::OrderRequest& order_request) const {
		std::ignore = order_request;
	}
	OrderStatus evaluate_risk(const OutboundOrderRequest& order, std::stop_token stop_token) const;
	std::expected<void, Error> forward_oms_updates(std::stop_token stop_token);

public:
	Trader(TradeBuffer& trade_buffer, const SeqLock<BestBidOffer>& bbo,
		const SeqLock<RiskData>& risk_data, OrderFillUpdateBuffer& update_buffer, T strategy = {})
		: trade_buffer_(trade_buffer),
		bbo_(bbo),
		risk_data_(risk_data),
		update_buffer_(update_buffer),
		strategy_(std::move(strategy)) {}

	Trader(const Trader&) = delete;
	Trader& operator=(const Trader&) = delete;

	void start(std::stop_token stop_token);
};

template<typename T> requires Strategy<T>
std::expected<void, Error> Trader<T>::place_new_orders(std::stop_token stop_token) {
	while(auto* outbound = order_buffer_.try_front()) {
		const OrderStatus status = evaluate_risk(*outbound, stop_token);
		if(status == OrderStatus::Stopped) return {};
		if(status != OrderStatus::Good) {
			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) {
				logging::write<config::LOGGING>(
					"Trader", "Rejected order #{} during risk check ({}).",
					outbound->request.order_id, static_cast<unsigned>(status));
			}
				
			const OutboundOrderRequest rejected = *outbound;
			order_buffer_.pop();
			strategy_.notify_invalid(rejected, status, order_buffer_);
			continue;
		}

		send_to_exchange(outbound->request);

		exchange::OrderRequest* slot = trade_buffer_.try_get_head_ref();
		if(slot == nullptr) [[unlikely]] return std::unexpected(Error::TradeBufferFull);
		*slot = outbound->request;
		trade_buffer_.publish();
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>)
			logging::write<config::LOGGING>("Trader", "Forwarded order #{} to OMS.", outbound->request.order_id);
		order_buffer_.pop();
	}
	return {};
}

template<typename T> requires Strategy<T>
auto Trader<T>::evaluate_risk(const OutboundOrderRequest& outbound, std::stop_token stop_token) const -> OrderStatus {
	using exchange::OrderRequest;
	if(stop_token.stop_requested()) return OrderStatus::Stopped;
	const std::optional<RiskData> risk = risk_data_.read_latest(stop_token);
	if(!risk) return OrderStatus::Stopped;

	// Risk-reducing requests remain available during a halt or rate breach so
	// the strategy can get flat.  All other requests are blocked by those gates.
	const OrderRequest& request = outbound.request;
	const bool reduces_exposure = request.type == OrderRequest::Type::Cancel ||
		(request.type == OrderRequest::Type::Update && outbound.quantity_change == QuantityChange::Reducing);

	// Halt Check
	if(risk->is_halted && !reduces_exposure) return OrderStatus::Halted;

	// Rate Limit
	if(!reduces_exposure && risk->orders_sent_in_window >= risk->max_orders_per_window) return OrderStatus::TooManyOrders;
	if(reduces_exposure) return OrderStatus::Good;

	// Position Limit
	RiskData::Position signed_delta = 0;
	if(request.type == OrderRequest::Type::Buy) signed_delta = request.quantity;
	else if(request.type == OrderRequest::Type::Sell) signed_delta = -static_cast<RiskData::Position>(request.quantity);
	else if(request.type == OrderRequest::Type::Update && outbound.quantity_change == QuantityChange::Increasing) {
		const RiskData::Position delta = request.quantity - outbound.previous_quantity;
		signed_delta = outbound.side == Order::Side::Buy ? delta : -delta;
	}

	if(std::abs(risk->net_position + signed_delta) > risk->max_position) {
		return OrderStatus::TooMuchExposure;
	}

	// Loss Limit
	if(risk->realized_pnl + risk->unrealized_pnl <= risk->max_loss_allowed) {
		return OrderStatus::TooMuchLoss;
	}

	return OrderStatus::Good;
}

template<typename T> requires Strategy<T>
std::expected<void, Error> Trader<T>::forward_oms_updates(std::stop_token stop_token) {
	while(auto* update = update_buffer_.try_get_tail_ref()) {
		strategy_.update(*update, order_buffer_);
		update_buffer_.consume();
		auto result = place_new_orders(stop_token);
		if(!result) return result;
	}
	return {};
}

template<typename T> requires Strategy<T>
void Trader<T>::start(std::stop_token stop_token) {
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>)
		logging::write<config::LOGGING>("Trader", "Started.");

	while(!stop_token.stop_requested()) {
		// Forward any order updates from the OMS to the strategy
		if(auto result = forward_oms_updates(stop_token); !result) {
			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>)
				logging::write<config::LOGGING>("Trader", "Stopping: {}.", result.error());
			return;
		}

		// Capture the newest BBO snapshot
		const auto snapshot = bbo_.wait_read_next(stop_token);
		if(!snapshot) return;

		// Inform the strategy, and place new orders off the new BBO
		strategy_.get_orders(*snapshot, order_buffer_);
		if(auto result = place_new_orders(stop_token); !result) {
			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>)
				logging::write<config::LOGGING>("Trader", "Stopping: {}.", result.error());
			return;
		}
	}

	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>)
		logging::write<config::LOGGING>("Trader", "Stopped.");
}
}

#endif
