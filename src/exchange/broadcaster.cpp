#include "broadcaster.hpp"
#include "config.hpp"
#include "encoded-message.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>
#include <fcntl.h>
#include <cerrno>
#include <cassert>
#include <cstdio>
#include <print>

namespace exchange {
auto Broadcaster::start() -> std::expected<void, Error> {
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if(socket_fd_ == INVALID) return std::unexpected(Error::StartBroadcast);

    const int flags = fcntl(socket_fd_, F_GETFL, 0);
    if(flags == INVALID || fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) == INVALID) {
        return std::unexpected(Error::StartBroadcast);
    }

	// Suppress SIGPIPE on macOS/BSD and relay an error instead
	#if defined(SO_NOSIGPIPE)
		constexpr int NO_SIGPIPE = 1;
		if(setsockopt(socket_fd_, SOL_SOCKET, SO_NOSIGPIPE, &NO_SIGPIPE, sizeof(NO_SIGPIPE)) == INVALID) {
			return std::unexpected(Error::StartBroadcast);
		}
	#endif

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

	destination_ = {};
    destination_.sin_family	= AF_INET;
	destination_.sin_port	= htons(config::MCAST_PORT);
	status = inet_pton(AF_INET, config::MCAST_GROUP, &destination_.sin_addr);
	if(status == 0) {
		return std::unexpected(Error::InvalidBroadcastIP);
	}
	if(status == INVALID) {
		return std::unexpected(Error::StartBroadcast);
	}

	in_addr multicast_interface {};
	if(inet_pton(AF_INET, config::MCAST_INTERFACE, &multicast_interface) != 1) {
		return std::unexpected(Error::StartBroadcast);
	}

	
	if(multicast_interface.s_addr != htonl(INADDR_ANY)) {
		ifaddrs* addresses = nullptr;
		if(getifaddrs(&addresses) == INVALID) {
			return std::unexpected(Error::StartBroadcast);
		}

		unsigned int interface_index = 0;
		for(const ifaddrs* address = addresses; address != nullptr; address = address->ifa_next) {
			if(address->ifa_addr == nullptr || address->ifa_addr->sa_family != AF_INET) {
				continue;
			}

			const auto* ipv4_address = reinterpret_cast<const sockaddr_in*>(address->ifa_addr);
			if(ipv4_address->sin_addr.s_addr == multicast_interface.s_addr) {
				interface_index = if_nametoindex(address->ifa_name);
				break;
			}
		}
		freeifaddrs(addresses);

		if(interface_index == 0) {
			return std::unexpected(Error::InvalidBroadcastInterface);
		}

		#if defined(IP_BOUND_IF)
			if(setsockopt(socket_fd_, IPPROTO_IP, IP_BOUND_IF, &interface_index, sizeof(interface_index)) == INVALID) {
				return std::unexpected(Error::StartBroadcast);
			}
		#endif
	}

	status = setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_IF, &multicast_interface, sizeof(multicast_interface));
	if(status == INVALID) {
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
