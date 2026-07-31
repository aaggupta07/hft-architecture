#include "tcp-retransmit.hpp"
#include "tcp-connection.hpp"
#include "encoded-message.hpp"
#include "network-utils.hpp"
#include "log.hpp"

#include <array>

#include <sys/socket.h>
#include <sys/event.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <cassert>
#include <tuple>

namespace exchange {

RetransmitRequest RetransmitRequest::parse(const std::span<std::byte> wire) {
	assert(wire.size() == PACKED_SIZE);

	RetransmitRequest request;
	std::memcpy(&request.first_packet, wire.data(), sizeof(SequenceID));
	std::memcpy(&request.last_packet, wire.data() + sizeof(SequenceID), sizeof(SequenceID));

	request.first_packet = ntohll(request.first_packet);
	request.last_packet = ntohll(request.last_packet);
	return request;
}

auto RetransmitRequest::serialize(const RetransmitRequest &request) -> std::array<std::byte, PACKED_SIZE> {
	std::array<std::byte, PACKED_SIZE> buffer;
	RetransmitRequest network_order {
		.first_packet = htonll(request.first_packet),
		.last_packet = htonll(request.last_packet),
	};
	std::memcpy(buffer.data(), &network_order.first_packet, sizeof(first_packet));
	std::memcpy(buffer.data() + sizeof(first_packet), &network_order.last_packet, sizeof(last_packet));
	return buffer;
}

auto RetransmitServer::get_listener() -> std::expected<int, Error> {
	sockaddr_in server_address {};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(config::RETRANSMIT_PORT);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	SocketFD new_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(new_socket == INVALID) [[unlikely]] {
		return std::unexpected(Error::StartRetransmitServer);
	}

	if(!network::set_socket_nonblocking(new_socket)) [[unlikely]] {
		close(new_socket);
		return std::unexpected(Error::SetSocketNonblocking);
	}

	if(bind(new_socket, reinterpret_cast<const sockaddr*>(&server_address), sizeof(server_address)) == INVALID) [[unlikely]] {
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
	if(event_queue_ >= 0) network::unregister_read_event(event_queue_, client.connection.socket());
	client.connection.close();
	client.request = std::nullopt;
	--current_connections_;
}

auto RetransmitServer::initialize() -> std::expected<void, Error> {
	// Set up a (nonblocking) listener
	{
		auto result = get_listener();
		if(!result) [[unlikely]] {
			return std::unexpected(result.error());
		}
		socket_fd_ = *result;
		current_connections_ = 1;
	}

	// Create event queue [BSD/macOS only]
	event_queue_ = kqueue();
	if(event_queue_ == INVALID) [[unlikely]] {
		return std::unexpected(Error::EventQueue);
	} 

	if(!network::register_read_event(event_queue_, socket_fd_)) [[unlikely]] return std::unexpected(Error::RegisterEvent);
	return {};
}

void RetransmitServer::close_server() noexcept {
	if(socket_fd_ >= 0) {
		close(socket_fd_);
		socket_fd_ = INVALID;
	}
	for(SavedConnection& client: connection_buffers) {
		if(client.connection.socket() >= 0) {
			notify_and_close(client, Error::ServerFatal);
		}
	}
	if(event_queue_ >= 0) {
		close(event_queue_);
		event_queue_ = INVALID;
	}
}

auto RetransmitServer::get_connected_socket() -> std::expected<SocketFD, Error> {
	if(current_connections_ >= config::MAX_TOTAL_CONNECTIONS) {
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


	if(!network::set_socket_nonblocking(connected_socket)) {
		close(connected_socket);
		return std::unexpected(Error::SetSocketNonblocking);
	}

	if(!network::register_read_event(event_queue_, connected_socket)) {
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
					if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("TCP Retransmit", "Error: {}", result.error());
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
		current_connections_++;
	}
	no_more_connections:
		return {};
}

auto RetransmitServer::run_event_loop(std::stop_token stop_token) -> std::expected<void, Error> {
	std::array<struct kevent, config::MAX_TOTAL_CONNECTIONS * 2> events;
	constexpr timespec stop_check_interval { .tv_sec = 0, .tv_nsec = config::KQUEUE_TIMEOUT_NS };

	while(!stop_token.stop_requested()) {
		int num_of_events = kevent(event_queue_, nullptr, 0, events.data(), events.size(), &stop_check_interval);
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
				if(!result) [[unlikely]] if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("TCP Retransmit", "Error: {}", result.error());
			}
		}
	}

	close_server();
	return {};
}

auto RetransmitServer::start(std::stop_token stop_token) -> std::expected<void, Error> {
	if(socket_fd_ == INVALID) {
		auto result = initialize();
		if(!result) [[unlikely]] return std::unexpected(result.error());
	}

	int status = listen(socket_fd_, config::MAX_PENDING_CONNECTIONS);
	if(status == INVALID) [[unlikely]] {
		return std::unexpected(Error::StartRetransmitServer);
	}

	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("TCP Retransmit", "Server started.");
	return run_event_loop(stop_token);
}


auto RetransmitServer::stream_packets(SavedConnection& client) -> std::expected<void, Error> {
	assert(client.request.has_value());
	assert(client.connection.socket() >= 0);
	if(client.request->first_packet < config::FIRST_SEQUENCE_ID) {
		return std::unexpected(Error::InvalidPacket);
	}

	if(client.connection.status() == Connection<N>::Status::PartialSend) {
		auto result = client.connection.send();
		if(!result) {
			return std::unexpected(result.error());
		}
		client.request->first_packet++;
	}

	for(; client.request->first_packet < client.request->last_packet; ++client.request->first_packet) {
		// The sequencer begins numbering at FIRST_SEQUENCE_ID, so packet FIRST_SEQUENCE_ID maps to slot 0 
		auto cache_result = retransmit_cache_.try_get_item(client.request->first_packet - config::FIRST_SEQUENCE_ID);
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
	auto result = client.connection.receive(RetransmitRequest::PACKED_SIZE);
	if(!result) {
		return std::unexpected(result.error());
	}
	client.request = RetransmitRequest::parse(*result);
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) {
		logging::write<config::LOGGING>("TCP Retransmit", "Received retransmit request for packets {} to {}.", client.request->first_packet, client.request->last_packet);
	}
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
								if(!network::register_oneshot_write_event(event_queue_, connected_socket)) {
									notify_and_close(client, Error::ClientConnection);
									return std::unexpected(Error::RegisterEvent);
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
