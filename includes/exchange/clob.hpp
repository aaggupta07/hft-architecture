#ifndef CLOB_HPP
#define CLOB_HPP

#include "protocol.hpp"
#include "exchange-errors.hpp"
#include "book-state.hpp"
#include "order.hpp"
#include <vector>
#include <optional>
#include <expected>

namespace exchange {
class CentralLimitOrderBook {
private:
	BookState state_;

	std::expected<std::vector<MarketEvent>, Error> cancel_order(const OrderRequest& request);
	std::expected<std::vector<MarketEvent>, Error> buy_order(const OrderRequest& request);
	std::expected<std::vector<MarketEvent>, Error> sell_order(const OrderRequest& request);

public:
	std::expected<std::vector<MarketEvent>, Error> submit(const OrderRequest& request);
	std::optional<Order::ID> sample_resting_order() const; 
};
}

#endif