#include "broadcaster.hpp"
#include "config.hpp"
#include "encoded-message.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cassert>
#include <print>

namespace exchange {
auto Broadcaster::start() -> std::expected<void, Error> {
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_fd_ == INVALID) return std::unexpected(Error::StartBroadcast);

    // Messages won't leave the local subnet
    constexpr uint8_t TTL = 1; 
	int status = setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_TTL, &TTL, sizeof(TTL));
    if(status == INVALID) {
        return std::unexpected(Error::StartBroadcast);
    }
	// Allow loopback to local device
    constexpr uint8_t LOOP = true;
	status = setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &LOOP, sizeof(LOOP));
    if(status == INVALID) {
        return std::unexpected(Error::StartBroadcast);
    }

    sockaddr_in destination{};
    destination.sin_family	= AF_INET;
	destination.sin_port	= htons(config::MCAST_PORT);
	status = inet_pton(AF_INET, config::MCAST_GROUP, &destination.sin_addr);
	if(status == 0) {
		return std::unexpected(Error::InvalidBroadcastIP);
	}
	if(status == INVALID) {
		return std::unexpected(Error::StartBroadcast);
	}

	status = connect(socket_fd_, reinterpret_cast<sockaddr*>(&destination), sizeof(destination));
	if(status == INVALID) {
		return std::unexpected(Error::StartBroadcast);
	}

	if constexpr(config::LOGGING) {
		std::println("[Broadcaster] Started");
	}

	return {};
}

auto Broadcaster::send(const std::span<const std::byte> message) -> std::expected<void, Error> {
	assert(message.size() <= EncodedMessage::MAX_WIRE_SIZE);
	int bytes_sent = ::send(socket_fd_, message.data(), message.size(), 0);
	if(bytes_sent == INVALID) return std::unexpected(Error::Send); 

	if constexpr(config::LOGGING) {
		std::println("[Broadcaster] Sent Message");
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