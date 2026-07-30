#include "network-utils.hpp"

#include <fcntl.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

Status join_multicast_group(SocketFD socket_fd, const char* mcast_group) noexcept {
	ip_mreq mreq{};
	const int status = inet_pton(AF_INET, mcast_group, &mreq.imr_multiaddr);
	if(status == 0) [[unlikely]] return Status::InvalidIPAddress;
	else if(status == -1) [[unlikely]] return Status::ProcedureError;
	mreq.imr_interface.s_addr = INADDR_ANY;
	return setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1 
				? Status::ProcedureError : Status::Good;
}
}
