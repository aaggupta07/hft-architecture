#ifndef UDP_LISTENER_HPP
#define UDP_LISTENER_HPP

#include "handler-errors.hpp"
#include "lazy-ring-buffer.hpp"
#include "encoded-message.hpp"

#include <stop_token>
#include <expected>
#include <netinet/in.h>

namespace handler {
class RealTimeListener {
private:
	using SocketFD = int;
	using PacketBuffer = LazyRingBuffer<exchange::EncodedMessage, config::UDP_PACKET_BUFFER_SIZE>;
	static constexpr int INVALID = -1;

	SocketFD socket_fd_ = INVALID;
	sockaddr_in local {};
	PacketBuffer& buffer_;

	std::expected<void, Error> initialize();
	std::expected<void, Error> run(std::stop_token stop_token);

public:
	RealTimeListener(PacketBuffer& buffer): buffer_(buffer) {}
	~RealTimeListener();

	std::expected<void, Error> start(std::stop_token stop_token);
};
}

#endif