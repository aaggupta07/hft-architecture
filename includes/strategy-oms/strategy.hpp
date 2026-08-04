#ifndef STRATEGY_HPP
#define STRATEGY_HPP

#include "book-state.hpp"
#include "protocol.hpp"
#include "lazy-ring-buffer.hpp"
#include "order.hpp"

#include <concepts>
#include <span>

namespace strategy {
using BestBidOffer = BookState::OrderSnapshot;
using TradeBuffer = LazyRingBuffer<exchange::OrderRequest, config::TRADE_BUFFER_SIZE>;
using MarketEventBuffer = LazyRingBuffer<MarketEvent, handler::config::MARKET_EVENT_BUFFER_SIZE>;

struct RiskData {
	// Contains whatever risk data we need
};

class OrderBookEngine;

class PassiveMarketMaker;

template<typename T, size_t N>
concept Strategy = requires(const BestBidOffer& bbo, std::span<exchange::OrderRequest> order_request_buffer) {
	{T::get_orders(bbo, order_request_buffer)} -> std::convertible_to<size_t>;
};
template<typename T, size_t N> requires Strategy<T,N>
class Trader;

class OrderManagementSystem;

};

#endif