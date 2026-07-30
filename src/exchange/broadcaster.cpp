#include "broadcaster.hpp"
#include "config.hpp"
#include "encoded-message.hpp"
#include "network-utils.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cerrno>
#include <cassert>
#include <cstdio>
#include <print>

namespace exchange {
auto Broadcaster::start() -> std::expected<void, Error> {
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_fd_ == INVALID) return std::unexpected(Error::StartBroadcast);

    if(	!network::set_socket_nonblocking(socket_fd_) ||
		!network::suppress_sigpipe(socket_fd_) ||
		!network::limit_multicast_to_local_subnet(socket_fd_) ||
		!network::enable_multicast_loopback(socket_fd_)) 
	{
		return std::unexpected(Error::StartBroadcast);
	}

    destination_.sin_family	= AF_INET;
	destination_.sin_port	= htons(config::MCAST_PORT);
	int status = inet_pton(AF_INET, config::MCAST_GROUP, &destination_.sin_addr);
	if(status == 0) return std::unexpected(Error::InvalidBroadcastIP);
	if(status == INVALID) return std::unexpected(Error::StartBroadcast);

	in_addr multicast_interface {};
	if(inet_pton(AF_INET, config::MCAST_INTERFACE, &multicast_interface) != 1) {
		return std::unexpected(Error::StartBroadcast);
	}

	if(!network::set_multicast_interface(socket_fd_, multicast_interface)) {
		return std::unexpected(Error::StartBroadcast);
	}

	if constexpr(config::LOGGING) {
		std::println("[Broadcaster] Started");
		std::fflush(stdout);
	}

	return {};
}

auto Broadcaster::send(const std::span<const std::byte> message) -> std::expected<void, Error> {
	assert(message.size() <= EncodedMessage::MAX_WIRE_SIZE);
	const ssize_t bytes_sent = sendto(socket_fd_, message.data(), message.size(), 0,
		reinterpret_cast<const sockaddr*>(&destination_), sizeof(destination_));
	if(bytes_sent == INVALID && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		return std::unexpected(Error::WouldBlock);
	}
	if(bytes_sent == INVALID || static_cast<size_t>(bytes_sent) != message.size()) {
		return std::unexpected(Error::Send);
	}

	if constexpr(config::LOGGING) {
		std::println("[Broadcaster] Sent message.");
		std::fflush(stdout);
	}
	return {};
}

Broadcaster::~Broadcaster() {
	if(socket_fd_ >= 0) {
		close(socket_fd_);
		socket_fd_ = INVALID;
	}

}

}
