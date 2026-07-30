#ifndef HANDLER_ERRORS_HPP
#define HANDLER_ERRORS_HPP

#include <format>
#include <cerrno>
#include <cstring>
#include <string_view>

namespace handler {
enum class Error {
	StartUDPListener,
	InvalidMulticastGroup,
	UDPListen,

};
}

template<>
struct std::formatter<handler::Error> : std::formatter<std::string_view> {
	auto format(const handler::Error& error, std::format_context& context) const {
		using enum handler::Error;
		std::string_view name;
		switch(error) {
			case StartUDPListener:
				name = "Start UDP Listener";
				break;
			case UDPListen:
				name = "UDP Listen Error";
				break;
			case InvalidMulticastGroup:
				name = "Invalid Multicast Group";
				break;
			default:
				name = "Unknown Error";
				break;
		}

		switch(error) {
			case StartUDPListener:
			case UDPListen:
				{
					const int error_number = errno;
					return std::format_to(context.out(), "{} (errno {}: {})", name,
											error_number, std::strerror(error_number));
				}
			default:
				return std::formatter<std::string_view>::format(name, context);
		}
	}
};



#endif