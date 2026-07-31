#ifndef NETWORK_UTILS_HPP
#define NETWORK_UTILS_HPP

#include <span>
#include <netinet/in.h>
#include <sys/socket.h>

namespace network {
using SocketFD = int;
using EventQueue = int;
static constexpr int INVALID = -1;

enum class JoinStatus {
	Success,
	InvalidIPAddress,
	ProcedureError,
};

enum class ReceiveStatus {
	Success,
	ConnectionClosed,
	Error,
};

bool set_socket_nonblocking(SocketFD socket_fd) noexcept;
bool register_read_event(EventQueue kq, SocketFD socket_fd) noexcept;
bool register_oneshot_write_event(EventQueue kq, SocketFD socket_fd) noexcept ;
bool unregister_read_event(EventQueue kq, SocketFD socket_fd) noexcept;

bool limit_multicast_to_local_subnet(SocketFD socket_fd) noexcept;
bool enable_multicast_loopback(SocketFD socket_fd) noexcept;
bool set_multicast_interface(SocketFD socket_fd, const in_addr& interface) noexcept;

bool enable_udp_port_sharing(SocketFD socket_fd) noexcept;
JoinStatus join_multicast_group(SocketFD socket_fd, const char* mcast_group) noexcept;

bool resize_recv_buffer(SocketFD socket_fd, int new_size_in_bytes) noexcept;

bool send_range(SocketFD socket_fd, const std::span<const std::byte> buffer, size_t& start, size_t end);
// Intentional - use when the start index is not cared about
inline bool send_range(SocketFD socket_fd, const std::span<const std::byte> buffer, size_t&& start, size_t end) {
	return send_range(socket_fd, buffer, start, end);
}

ReceiveStatus recv_range(SocketFD socket_fd, const std::span<std::byte> buffer, size_t& start, size_t end);
inline ReceiveStatus recv_range(SocketFD socket_fd, const std::span<std::byte> buffer, size_t&& start, size_t end) {
	return recv_range(socket_fd, buffer, start, end);
}

// SO_NOSIGPIPE reports a broken peer as an error instead of raising SIGPIPE.
inline bool suppress_sigpipe(SocketFD socket_fd) noexcept {
	#if defined(SO_NOSIGPIPE)
		constexpr int NO_SIGPIPE = 1;
		if(setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE, &NO_SIGPIPE, sizeof(NO_SIGPIPE)) == INVALID) [[unlikely]] {
			return false;
		}
		return true;
	#endif
	return true;
}
}

#endif
