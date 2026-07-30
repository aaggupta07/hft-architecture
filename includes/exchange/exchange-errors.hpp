#ifndef EXCHANGE_ERRORS_HPP
#define EXCHANGE_ERRORS_HPP

#include <format>
#include <cerrno>
#include <cstring>
#include <string_view>

namespace exchange {
enum class Error {
	// Public TCP Retransmit Errors
    PacketTooOld 		= 1,
	InvalidPacket 		= 2,
	PacketUnavailable 	= 3,
	ClientConnection	= 4,
	ServerFatal 		= 5,

	// Public Order Request Errors
	OrderDoesNotExist 	= 6,
	InvalidOrderRequest = 7,

	StartBroadcast,
	InvalidBroadcastIP,
	InvalidBroadcastInterface,
    Send,

	ServerBusy,
	StartRetransmitServer,
	AddressInfo,
	SetSocketNonblocking,
	ClientConnectionClosed,
	WouldBlock,
	ReceiveFromClient,
	SendToClient,
	NewConnection,
	RequestHandler,

	RegisterEvent,
	EventQueue,
};

}

template<>
struct std::formatter<exchange::Error> : std::formatter<std::string_view> {
	auto format(const exchange::Error& error, std::format_context& context) const {
		using enum exchange::Error;
		std::string_view name;
		switch(error) {
			case PacketTooOld:
				name = "PacketTooOld";
				break;
			case InvalidPacket:
				name = "InvalidPacket";
				break;
			case PacketUnavailable:
				name = "PacketUnavailable";
				break;
			case ClientConnection:
				name = "ClientConnection";
				break;
			case ServerFatal:
				name = "ServerFatal";
				break;
			case StartBroadcast:
				name = "StartBroadcast";
				break;
			case Send:
				name = "Send";
				break;
			case ServerBusy:
				name = "ServerBusy";
				break;
			case StartRetransmitServer:
				name = "StartRetransmitServer";
				break;
			case AddressInfo:
				name = "AddressInfo";
				break;
			case SetSocketNonblocking:
				name = "SetSocketNonblocking";
				break;
			case ClientConnectionClosed:
				name = "ClientConnectionClosed";
				break;
			case WouldBlock:
				name = "WouldBlock";
				break;
			case ReceiveFromClient:
				name = "ReceiveFromClient";
				break;
			case SendToClient:
				name = "SendToClient";
				break;
			case NewConnection:
				name = "NewConnection";
				break;
			case RequestHandler:
				name = "RequestHandler";
				break;
			case RegisterEvent:
				name = "RegisterEvent";
				break;
			case EventQueue:
				name = "EventQueue";
				break;
			case OrderDoesNotExist:
				name = "OrderDoesNotExist";
				break;
			case InvalidOrderRequest:
				name = "InvalidOrderRequest";
				break;
			case InvalidBroadcastIP:
				name = "InvalidBroadcastIP";
				break;
			case InvalidBroadcastInterface:
				name = "InvalidBroadcastInterface";
				break;
			default:
				name = "UnknownError";
				break;
		}

		switch(error) {
			case StartBroadcast:
			case Send:
			case StartRetransmitServer:
			case SetSocketNonblocking:
			case ClientConnectionClosed:
			case WouldBlock:
			case ReceiveFromClient:
			case SendToClient:
			case NewConnection:
			case RegisterEvent:
			case EventQueue:
			case ServerFatal:
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
