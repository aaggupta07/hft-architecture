#include "tcp-retransmit.hpp"
#include "tcp-connection.hpp"
#include "encoded-message.hpp"

#include <array>

#include <sys/socket.h>
#include <sys/event.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <cassert>

namespace exchange {

RetransmitRequest RetransmitRequest::parse(const std::span<Byte> wire) {
	assert(wire.size() == sizeof(RetransmitRequest));

	RetransmitRequest request;
	std::memcpy(&request.first_packet, wire.data(), sizeof(SequenceID));
	std::memcpy(&request.last_packet, wire.data() + sizeof(SequenceID), sizeof(SequenceID));

	request.first_packet = ntohll(request.first_packet);
	request.last_packet = ntohll(request.last_packet);
	return request;
}

auto RetransmitRequest::serialize(const RetransmitRequest &request) {
	std::array<Byte, sizeof(RetransmitRequest)> buffer;
	RetransmitRequest network_order {
		.first_packet = htonll(request.first_packet),
		.last_packet = htonll(request.last_packet),
	};
	std::memcpy(buffer.data(), &network_order.first_packet, sizeof(first_packet));
	std::memcpy(buffer.data() + sizeof(first_packet), &network_order.last_packet, sizeof(last_packet));
	return buffer;
}

std::expected<void, Error> RetransmitServer::set_socket_nonblocking(int socket_fd) {
	int flags = fcntl(socket_fd, F_GETFL, 0);
	if(flags == INVALID) [[unlikely]] {
		return std::unexpected(Error::SetSocketNonblocking);
	}
	if(fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == INVALID) [[unlikely]] {
		return std::unexpected(Error::SetSocketNonblocking);
	}
	return {};
}

std::expected<void, Error> RetransmitServer::register_read_event(int kq, int socket_fd) {
	struct kevent event;
	EV_SET(&event, socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
	if(kevent(kq, &event, 1, nullptr, 0, nullptr) == INVALID) [[unlikely]] {
		return std::unexpected(Error::RegisterEvent);
	}
	return {};
}

std::expected<void, Error> RetransmitServer::register_write_event(int kq, int socket_fd) {
	struct kevent event;
	EV_SET(&event, socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, nullptr);
	if(kevent(kq, &event, 1, nullptr, 0, nullptr) == INVALID) [[unlikely]] {
		return std::unexpected(Error::RegisterEvent);
	}
	return {};
}

void RetransmitServer::unregister(int kq, int socket_fd) {
	struct kevent event;
	EV_SET(&event, socket_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
	kevent(kq, &event, 1, nullptr, 0, nullptr); // best effort
}

auto RetransmitServer::get_listener() -> std::expected<int, Error> {
	addrinfo hints {};
	addrinfo* server_info = nullptr;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(nullptr, RETRANSMIT_PORT, &hints, &server_info);
	if(status != 0) [[unlikely]] {
		return std::unexpected(Error::AddressInfo);
	}
	
	SocketFD new_socket = socket(server_info->ai_family, server_info->ai_socktype, 0);
	if(new_socket == INVALID) [[unlikely]] {
		freeaddrinfo(server_info);
		return std::unexpected(Error::StartRetransmitServer);
	}

	auto result = set_socket_nonblocking(new_socket);
	if(!result) [[unlikely]] {
		close(new_socket);
		freeaddrinfo(server_info);
		return std::unexpected(result.error());
	}

	status = bind(new_socket, server_info->ai_addr, server_info->ai_addrlen);
	freeaddrinfo(server_info);

	if(status == INVALID) [[unlikely]] {
		close(new_socket);
		return std::unexpected(Error::StartRetransmitServer);
	}

	return new_socket;
}

void RetransmitServer::close_client(SavedConnection& client)  {
	unregister(event_queue_, client.connection.socket());
	client.connection.close();
	client.request = std::nullopt;
}

auto RetransmitServer::initialize() -> std::expected<void, Error> {
	// Set up a (nonblocking) listener
	{
		auto result = get_listener();
		if(!result) [[unlikely]] {
			return std::unexpected(result.error());
		}
		socket_fd_ = *result;
	}

	// Create event queue [BSD/macOS only]
	event_queue_ = kqueue();
	if(event_queue_ == INVALID) [[unlikely]] {
		return std::unexpected(Error::EventQueueError);
	} 

	return register_read_event(event_queue_, socket_fd_);
}

auto RetransmitServer::run_event_loop() -> std::expected<void, Error> {
	// TODO
	return {};
}

auto RetransmitServer::start() -> std::expected<void, Error> {
	if(socket_fd_ == INVALID) [[unlikely]] {
		auto result = initialize();
		if(!result) return std::unexpected(result.error());
	}
	int status = listen(socket_fd_, MAX_PENDING_CONNECTIONS);
	if(status == INVALID) [[unlikely]] {
		return std::unexpected(Error::StartRetransmitServer);
	}

	return run_event_loop();
}


auto RetransmitServer::stream_packets(SavedConnection& client) -> std::expected<void, Error> {
	assert(client.request.has_value());
	assert(client.connection.socket() >= 0);

	if(client.connection.status() == Connection<N>::Status::PartialSend) {
		auto result = client.connection.send();
		if(!result) {
			return std::unexpected(result.error());
		}
		client.request->first_packet++;
	}

	for(; client.request->first_packet < client.request->last_packet; ++client.request->first_packet) {
		auto cache_result = retransmit_cache_.try_get_item(client.request->first_packet);
		if(!cache_result) {
			switch(cache_result.error()) {
				case Cache::Error::DataTooOld:
					return std::unexpected(Error::PacketTooOld);
				case Cache::Error::DataDoesNotExist:
					return std::unexpected(Error::InvalidPacket);
				case Cache::Error::ConcurrentWrite:
					return std::unexpected(Error::PacketUnavailable);
			}
		}
		client.connection.save(cache_result->message());
		auto send_result = client.connection.send();
		if(!send_result) return send_result;
	}

	client.request = std::nullopt;
	return {};
}

auto RetransmitServer::receive_request(SavedConnection& client) -> std::expected<void, Error> {
	auto result = client.connection.receive(sizeof(RetransmitRequest));
	if(!result) {
		return std::unexpected(result.error());
	}
	client.request = RetransmitRequest::parse(*result);
	return {};
}

auto RetransmitServer::handle_request(SocketFD connected_socket) -> std::expected<void, Error> {
	assert(connected_socket >= 0);
	assert(static_cast<size_t>(connected_socket) < connection_buffers.size());
	SavedConnection& client = connection_buffers.at(connected_socket);
	
	switch(client.connection.status()) {
		case Connection<N>::Status::Clear:
			[[fallthrough]];
		case Connection<N>::Status::PartialReceive:
			{ 
				auto result = receive_request(client);
				if(!result) return result;
			}
			[[fallthrough]];
		case Connection<N>::Status::SavedMessage:
			[[fallthrough]];
		case Connection<N>::Status::PartialSend:
			{
				auto result = stream_packets(client);
				if(!result) return result;
			}
			break;
	}

	return {};
}


}