#ifndef PUBLISHER_HPP
#define PUBLISHER_HPP

#include "handler.hpp"
#include "order.hpp"
#include "lazy-ring-buffer.hpp"
#include "reorder-buffer.hpp"

#include <stop_token>
#include <print>
#include <cstdio>
#include <string_view>

namespace handler {
template<BinaryDecoder Decoder>
class MarketEventPublisher {
private:
	MarketReorderBuffer& reorder_buffer_;
	MarketEventBuffer& order_buffer_;

	static constexpr void log(std::string_view message) {
		std::println("[Market Event Publisher] {}", message);
		std::fflush(stdout);
	}

public:
	constexpr MarketEventPublisher(MarketReorderBuffer& reorder_buffer, MarketEventBuffer& order_buffer)
		: reorder_buffer_(reorder_buffer), order_buffer_(order_buffer) {}

	void run(std::stop_token stop_token) {
		while(!stop_token.stop_requested()) {
			std::optional<SerializedMarketEvent> result = reorder_buffer_.wait_consume_next(stop_token);
			if(!result) [[unlikely]] return;
			if constexpr(config::LOGGING) log("Pulled packet data from reorder buffer.");

			MarketEvent* slot = order_buffer_.wait_get_head_ref(stop_token);
			if(slot == nullptr) [[unlikely]] return;
			*slot = Decoder::decode(*result);
			order_buffer_.publish();
			if constexpr(config::LOGGING) log("Published market event.");
		}
	}
};
}

#endif
