#ifndef STRATEGY_ERRORS_HPP
#define STRATEGY_ERRORS_HPP

#include <cstdint>
#include <format>
#include <string_view>

namespace strategy {
enum class Error: uint8_t {
	CriticalRiskLimitBreached,
	TradeBufferFull,
	TooManyOrders,
};
}

template<>
struct std::formatter<strategy::Error> : std::formatter<std::string_view> {
	auto format(const strategy::Error& error, std::format_context& context) const {
		using enum strategy::Error;
		std::string_view name;
		switch(error) {
			case CriticalRiskLimitBreached: name = "Critical risk limit breached"; break;
			case TradeBufferFull: name = "Trade buffer full"; break;
			case TooManyOrders: name = "Too many orders"; break;
			default: name = "Unknown strategy error"; break;
		}
		return std::formatter<std::string_view>::format(name, context);
	}
};

#endif
