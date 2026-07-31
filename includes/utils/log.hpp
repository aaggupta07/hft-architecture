#ifndef LOG_HPP
#define LOG_HPP

#include "config.hpp"

#include <cstdio>
#include <format>
#include <print>
#include <string_view>
#include <utility>

namespace logging {
using Setting = ::config::LogSetting;
inline constexpr size_t MINIMAL_INTERVAL = 5'000;

template<Setting Current, Setting Minimum>
inline constexpr bool enabled = static_cast<unsigned>(Current) >= static_cast<unsigned>(Minimum);

template<Setting Current, typename... Args>
inline void write(std::string_view component, std::format_string<Args...> format, Args&&... args) {
	std::print("[{}] ", component);
	std::println(format, std::forward<Args>(args)...);
	if constexpr(Current == Setting::Flush) std::fflush(stdout);
}
}

#endif
