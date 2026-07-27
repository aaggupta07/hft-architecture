#ifndef EXCHANGE_ERRORS_HPP
#define EXCHANGE_ERRORS_HPP

#include <format>

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
struct std::formatter<exchange::Error> {
	constexpr auto parse(std::format_parse_context& context) {
		return context.begin();
	}

	auto format(const exchange::Error& error, std::format_context& _) const {
		using enum exchange::Error;
		switch(error) {
			case PacketTooOld:
				return "PacketTooOld";
			case InvalidPacket:
				return "InvalidPacket";
			case PacketUnavailable:
				return "PacketUnavailable";
			case ClientConnection:
				return "ClientConnection";
			case ServerFatal:
				return "ServerFatal";		
			case StartBroadcast:
				return "StartBroadcast";
			case Send:
				return "Send";
			case ServerBusy:
				return "ServerBusy";
			case StartRetransmitServer:
				return "StartRetransmitServer";
			case AddressInfo:
				return "AddressInfo";
			case SetSocketNonblocking:
				return "SetSocketNonblocking";
			case ClientConnectionClosed:
				return "ClientConnectionClosed";
			case WouldBlock:
				return "WouldBlock";
			case ReceiveFromClient:
				return "ReceiveFromClient";
			case SendToClient:
				return "SendToClient";
			case NewConnection:
				return "NewConnection";
			case RequestHandler:
				return "RequestHandler";
			case RegisterEvent:
				return "RegisterEvent";
			case EventQueue:
				return "EventQueue";
			case OrderDoesNotExist:
				return "OrderDoesNotExist";
			case InvalidOrderRequest:
				return "InvalidOrderRequest";
		}
	}
};

#endif