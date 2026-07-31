#ifndef HANDLER_ERRORS_HPP
#define HANDLER_ERRORS_HPP

#include <format>
#include <cerrno>
#include <cstring>
#include <string_view>

namespace handler {
enum class Error {
	// Real-Time Feed Listener (UDP)
	StartUDPListener,
	InvalidMulticastGroup,
	UDPListen,
	HandlerTooSlow,

	// Retransmit Client (TCP)
	StartRetransmitClient,
	InvalidRetransmitServer,
	ConnectToRetransmitServer,
	RetransmitServer,
	RetransmitClientSend,
	RetransmitClientReceive,

	// Market Data Handler
	ReaderTooFarBehind,
	WriterTooFarBehind,
	RetransmitRequestBufferFull,
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
				name = "Invalid Multicast Group IP";
				break;
			case StartRetransmitClient:
				name = "Start Retransmit Client";
				break;
			case ConnectToRetransmitServer:
				name = "Connect to Retransmit Server";
				break;
			case RetransmitServer:
				name = "Retransmit Server Error";
				break;
			case RetransmitClientSend:
				name = "Retransmit Client Send Error";
				break;
			case RetransmitClientReceive:
				name = "Retransmit Client Receive Error";
				break;
			case InvalidRetransmitServer:
				name = "Invalid Retransmit Server IP";
				break;
			case ReaderTooFarBehind:
				name = "Fatal: Market data handler too far behind. Likely slow reader.";
				break;
			case WriterTooFarBehind:
				name = "Fatal: Market data handler too far behind. Likely slow TCP retransmission.";
				break;
			case RetransmitRequestBufferFull:
				name = "Retransmit request buffer full.";
				break;
			case HandlerTooSlow:
				name = "Fatal: Market data handler too slow. Packet buffer is full.";
				break;
			default:
				name = "Unknown Error";
				break;
		}

		switch(error) {
			case StartUDPListener:
			case UDPListen:
			case InvalidMulticastGroup:
			case StartRetransmitClient:
			case ConnectToRetransmitServer:
			case RetransmitClientSend:
			case RetransmitClientReceive:
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