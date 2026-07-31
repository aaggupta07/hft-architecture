#include "network-utils.hpp"

#include <fcntl.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>

namespace network {
bool set_socket_nonblocking(SocketFD socket_fd) noexcept {
	const int flags = fcntl(socket_fd, F_GETFL, 0);
	if(flags == INVALID || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == INVALID) [[unlikely]] {
		return false;
	}
	return true;
}

bool register_read_event(EventQueue kq, SocketFD socket_fd) noexcept {
	struct kevent event;
	EV_SET(&event, socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
	if(kevent(kq, &event, 1, nullptr, 0, nullptr) == INVALID) [[unlikely]] {
		return false;
	}
	return true;
}

bool register_oneshot_write_event(EventQueue kq, SocketFD socket_fd) noexcept {
	struct kevent event;
	EV_SET(&event, socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, nullptr);
	if(kevent(kq, &event, 1, nullptr, 0, nullptr) == INVALID) [[unlikely]] {
		return false;
	}
	return true;
}

bool unregister_read_event(EventQueue kq, SocketFD socket_fd) noexcept {
	struct kevent event;
	EV_SET(&event, socket_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
	if(kevent(kq, &event, 1, nullptr, 0, nullptr) == INVALID) [[unlikely]] {
		return false;
	}
	return true;
}

bool limit_multicast_to_local_subnet(SocketFD socket_fd) noexcept {
	// IP_MULTICAST_TTL of one prevents multicast datagrams from leaving the local subnet.
	constexpr uint8_t TTL = 1;
	return setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &TTL, sizeof(TTL)) != INVALID;
}

bool enable_multicast_loopback(SocketFD socket_fd) noexcept {
	// IP_MULTICAST_LOOP delivers sent multicast datagrams to local receivers too.
	constexpr uint8_t LOOP = 1;
	return setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &LOOP, sizeof(LOOP)) != INVALID;
}

bool set_multicast_interface(SocketFD socket_fd, const in_addr& interface) noexcept {
	// IP_MULTICAST_IF selects the network interface used for outgoing multicast datagrams.
	return setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_IF, &interface, sizeof(interface)) != INVALID;
}

bool enable_udp_port_sharing(SocketFD socket_fd) noexcept {
	// SO_REUSEADDR lets multiple processes bind to the same port
	constexpr uint8_t MULTI_BIND = 1;
	return setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &MULTI_BIND, sizeof(MULTI_BIND)) != INVALID;
}

JoinStatus join_multicast_group(SocketFD socket_fd, const char* mcast_group) noexcept {
	ip_mreq mreq{};
	const int status = inet_pton(AF_INET, mcast_group, &mreq.imr_multiaddr);
	if(status == 0) [[unlikely]] return JoinStatus::InvalidIPAddress;
	else if(status == -1) [[unlikely]] return JoinStatus::ProcedureError;
	mreq.imr_interface.s_addr = INADDR_ANY;
	return setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == INVALID 
				? JoinStatus::ProcedureError : JoinStatus::Success;
}

bool resize_recv_buffer(SocketFD socket_fd, size_t new_size_in_bytes) noexcept {
	return setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &new_size_in_bytes, sizeof(new_size_in_bytes)) != INVALID;
}

bool send_range(SocketFD socket_fd, const std::span<const std::byte> buffer, size_t& start, size_t end) {
	int bytes_sent = 0;
	for(; start < end; start += bytes_sent) {
		bytes_sent = ::send(socket_fd, buffer.data() + start, end - start, 0);
		if(bytes_sent == 0) {
			errno = EPIPE;
			return false;
		}
		if(bytes_sent == INVALID) return false;
	}
	return true;
}

ReceiveStatus recv_range(SocketFD socket_fd, const std::span<std::byte> buffer, size_t& start, size_t end) {
	int bytes_received = 0;
	for(; start < end; start += bytes_received) {
		bytes_received = ::recv(socket_fd, buffer.data() + start, end - start, 0);
		if(bytes_received == 0) return ReceiveStatus::ConnectionClosed;
		if(bytes_received == INVALID) return ReceiveStatus::Error;
	}
	return ReceiveStatus::Success;
}

}
