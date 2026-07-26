#ifndef EXCHANGE_ERRORS_HPP
#define EXCHANGE_ERRORS_HPP

namespace exchange {
enum class Error {
    StartBroadcast,
    Send,

    PacketTooOld,
	InvalidPacket,
	PacketUnavailable,

	StartRetransmitServer,
	AddressInfo,
	SetSocketNonblocking,
	ClientConnectionClosed,
	WouldBlock,
	ReceiveFromClient,
	SendToClient,

	RegisterEvent,
	UnregisterEvent,
	EventQueueError,
};
}

#endif