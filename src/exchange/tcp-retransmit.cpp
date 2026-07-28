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
#include <print>
#include <tuple>

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

constexpr void RetransmitServer::log(const Error& error) const {
	std::println("{}", error);
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

void RetransmitServer::unregister(int kq, int socket_fd) noexcept {
	struct kevent event;
	EV_SET(&event, socket_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
	kevent(kq, &event, 1, nullptr, 0, nullptr); // best effort
}

auto RetransmitServer::get_listener() -> std::expected<int, Error> {
	addrinfo hints {};
	addrinfo* server_info = nullptr;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(nullptr, config::RETRANSMIT_PORT, &hints, &server_info);
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

void RetransmitServer::notify_and_close(SavedConnection& client, const Error& error) noexcept{
	MessageHeader error_header {
		.sequence_number = 0,
		.payload_length = static_cast<Length>(error),
	};
	auto message = EncodedMessage(error_header, {});
	client.connection.clear();
	client.connection.save(message.serialize());
	std::ignore = client.connection.send();
	close_client(client);
}

void RetransmitServer::close_client(SavedConnection& client) noexcept {
	if(event_queue_ >= 0) unregister(event_queue_, client.connection.socket());
	client.connection.close();
	client.request = std::nullopt;
	--current_connections;
}

auto RetransmitServer::initialize() -> std::expected<void, Error> {
	// Set up a (nonblocking) listener
	{
		auto result = get_listener();
		if(!result) [[unlikely]] {
			return std::unexpected(result.error());
		}
		socket_fd_ = *result;
		current_connections = 1;
	}

	// Create event queue [BSD/macOS only]
	event_queue_ = kqueue();
	if(event_queue_ == INVALID) [[unlikely]] {
		return std::unexpected(Error::EventQueue);
	} 

	return register_read_event(event_queue_, socket_fd_);
}

void RetransmitServer::close_server() noexcept {
	if(socket_fd_ >= 0) {
		close(socket_fd_);
		socket_fd_ = INVALID;
	}
	for(SavedConnection& client: connection_buffers) {
		notify_and_close(client, Error::ServerFatal);
	}
	if(event_queue_ >= 0) {
		close(event_queue_);
		event_queue_ = INVALID;
	}
}

auto RetransmitServer::get_connected_socket() -> std::expected<SocketFD, Error> {
	if(current_connections >= config::MAX_TOTAL_CONNECTIONS) {
		return std::unexpected(Error::ServerBusy);
	}

	SocketFD connected_socket = accept(socket_fd_, nullptr, nullptr);
	if(connected_socket == INVALID && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		return std::unexpected(Error::WouldBlock);
	}
	else if(connected_socket == INVALID && (errno == EINTR || errno == ECONNABORTED)) {
		return std::unexpected(Error::NewConnection);
	}
	else if(connected_socket == INVALID) {
		return std::unexpected(Error::ServerFatal);
	}


	auto result = set_socket_nonblocking(connected_socket);
	if(!result) {
		close(connected_socket);
		return std::unexpected(Error::SetSocketNonblocking);
	}

	result = register_read_event(event_queue_, connected_socket);
	if(!result) {
		close(connected_socket);
		return std::unexpected(Error::RegisterEvent);
	}

	return connected_socket;
}

auto RetransmitServer::handle_new_connections() -> std::expected<void, Error> {
	while(true) {
		auto result = get_connected_socket();
		if(!result) {
			switch(result.error()) {
				case Error::RegisterEvent: [[fallthrough]];
				case Error::SetSocketNonblocking:
					if constexpr(config::LOGGING) log(result.error());
					[[fallthrough]];
				case Error::NewConnection:
					continue;
				case Error::WouldBlock: [[fallthrough]];
				case Error::ServerBusy:
					goto no_more_connections;
				case Error::ServerFatal:
					return std::unexpected(result.error());
				default:
					assert(false && "[TCP Retransmit] Handle New Connection: Unreachable");
					std::unreachable();
			}
		}

		SocketFD new_socket = *result;
		while(static_cast<size_t>(new_socket) >= connection_buffers.size()) {
			connection_buffers.emplace_back(SavedConnection());
		}
		connection_buffers[new_socket].connection.set_socket(new_socket);
	}
	no_more_connections:
		return {};
}

auto RetransmitServer::run_event_loop() -> std::expected<void, Error> {
	std::array<struct kevent, config::MAX_TOTAL_CONNECTIONS * 2> events;

	while(true) {
		int num_of_events = kevent(event_queue_, nullptr, 0, events.data(), events.size(), nullptr);
		if(num_of_events == INVALID && errno == EINTR) continue;
		else if(num_of_events == INVALID) [[unlikely]]  {
			close_server();
			return std::unexpected(Error::EventQueue);
		}

		for(int i = 0; i < num_of_events; ++i) {
			const struct kevent& event = events[i];

			// Error with client event
			if((event.flags & EV_ERROR) && static_cast<SocketFD>(event.ident) != socket_fd_) {
				notify_and_close(connection_buffers[event.ident], Error::ClientConnection);
				continue;
			}
			// Error with listener (fatal)
			else if((event.flags & EV_ERROR)) [[unlikely]] {
				close_server();
				return std::unexpected(Error::EventQueue);
			}
			
			// New connection(s) on listener
			if(static_cast<SocketFD>(event.ident) == socket_fd_) {
				auto result = handle_new_connections();
				if(!result && result.error() == Error::ServerFatal) [[unlikely]] {
					close_server();
					return std::unexpected(Error::ServerFatal);
				}
			}

			// New event on existing connection
			else {
				auto result = handle_request(static_cast<SocketFD>(event.ident));
				if constexpr(config::LOGGING) {
					if(!result) log(result.error());
				}
			}
		}
	}

	close_server();
	return {};
}

auto RetransmitServer::start() -> std::expected<void, Error> {
	if(socket_fd_ == INVALID) [[unlikely]] {
		auto result = initialize();
		if(!result) return std::unexpected(result.error());
	}
	int status = listen(socket_fd_, config::MAX_PENDING_CONNECTIONS);
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
				if(!result) {
					switch(result.error()) {
						case Error::WouldBlock: // Drained partial message - rest is unavailable right now
							return {};
						case Error::ClientConnectionClosed:
							close_client(client);
							return std::unexpected(result.error());
						case Error::ReceiveFromClient:
							notify_and_close(client, Error::ClientConnection);
							return std::unexpected(result.error());
						default:
							assert(false && "[TCP Retransmit] Handler (receive request): Unreachable");
							std::unreachable();
					}
				}
			}
			[[fallthrough]];
		case Connection<N>::Status::SavedMessage:
			[[fallthrough]];
		case Connection<N>::Status::PartialSend:
			{
				auto result = stream_packets(client);
				if(!result) {
					switch(result.error()) {
						case Error::WouldBlock:
							{
								auto event_result = register_write_event(event_queue_, connected_socket);
								if(!event_result) {
									notify_and_close(client, Error::ClientConnection);
									return std::unexpected(event_result.error());
								}
								return {};
							}
						case Error::ClientConnectionClosed:
							close_client(client);
							return std::unexpected(result.error());
						case Error::PacketTooOld: [[fallthrough]];
						case Error::InvalidPacket: [[fallthrough]];
						case Error::PacketUnavailable: [[fallthrough]];
						case Error::SendToClient:
							// In this case, a send failed, so we can't notify the client
							close_client(client);
							return std::unexpected(result.error());
						default:
							assert(false && "[TCP Retransmit] Handler (send request): Unreachable");
							std::unreachable();
					}
				}
			}
			break;
	}

	return {};
}


}