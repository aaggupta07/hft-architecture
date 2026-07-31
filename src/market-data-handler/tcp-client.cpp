#include "tcp-client.hpp"
#include "network-utils.hpp"
#include "log.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace handler {
std::expected<void, Error> RetransmitListener::connect_to_server() {
	assert(socket_fd_ >= 0);

	sockaddr_in server_info {};
	server_info.sin_family = AF_INET;
	server_info.sin_port = htons(exchange::config::RETRANSMIT_PORT);
	int status = inet_pton(AF_INET, config::SERVER_IP, &server_info.sin_addr);

	if(status == 0) {
		return std::unexpected(Error::InvalidRetransmitServer);
	}
	if(status == INVALID) {
		return std::unexpected(Error::StartRetransmitClient);
	}

	if(connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_info), sizeof(server_info)) == INVALID) {
		return std::unexpected(Error::ConnectToRetransmitServer);
	}
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("Retransmit Client", "Connected to retransmit server.");

	return {};
}

std::expected<void, Error> RetransmitListener::initialize_and_connect() {
	socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd_ == -1) return std::unexpected(Error::StartRetransmitClient);
	return connect_to_server();
}

std::expected<void, Error> RetransmitListener::send_request_to_server(const exchange::RetransmitRequest& request) {
	auto serialized_request = exchange::RetransmitRequest::serialize(request);
	bool send_result = network::send_range(socket_fd_, serialized_request, 0, serialized_request.size());
	if(!send_result) return std::unexpected(Error::RetransmitClientSend);
	return {};
}

std::expected<void, Error> RetransmitListener::receive_and_decode_header(exchange::EncodedMessage& new_message) {
	network::ReceiveStatus recv_status = network::recv_range(socket_fd_, 
			new_message.get_buffer_ref(), 0, exchange::MessageHeader::PACKED_WIRE_SIZE);
	if(recv_status == network::ReceiveStatus::Error) {
		return std::unexpected(Error::RetransmitClientReceive);
	}

	// The server closed the connection without an error message
	if(recv_status == network::ReceiveStatus::ConnectionClosed) {
		return std::unexpected(Error::RetransmitServer);
	}

	new_message.decode_header_from_buffer();

	// The server provided an error message
	if(new_message.header().sequence_number == 0) {
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("Retransmit Client", "Exchange retransmit server error: {}", static_cast<exchange::Error>(new_message.header().payload_length));
		return std::unexpected(Error::RetransmitServer);
	}

	// A valid header was received
	return {};
}

std::expected<void, Error> RetransmitListener::receive_payload(exchange::EncodedMessage& new_message) {
	assert(new_message.header().payload_length > 0);
	assert(new_message.header().payload_length <= exchange::config::MAX_MESSAGE_BYTES);
	network::ReceiveStatus status = network::recv_range(socket_fd_, new_message.get_payload_ref(), 0, new_message.header().payload_length);
	switch(status) {
		case network::ReceiveStatus::ConnectionClosed:
			return std::unexpected(Error::RetransmitServer);
		case network::ReceiveStatus::Error:
			return std::unexpected(Error::RetransmitClientReceive);
		case network::ReceiveStatus::Success:
			return {};
	}
	assert(false && "[Retransmit Client] receive_payload: unreachable");
	std::unreachable();
}

std::expected<void, Error> RetransmitListener::handle_request(exchange::RetransmitRequest& request, std::stop_token stop_token) {
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) logging::write<config::LOGGING>("Retransmit Client", "Requesting packets {} to {}.", request.first_packet, request.last_packet);
	auto result = send_request_to_server(request);
	if(!result) return result;

	for(; request.first_packet < request.last_packet && !stop_token.stop_requested(); ++request.first_packet) {
		exchange::EncodedMessage new_message;

		// First, get the header
		auto result = receive_and_decode_header(new_message);
		if(!result) return result;
		// Then, get the packet
		result = receive_payload(new_message);
		if(!result) return result;

		assert(new_message.get_payload_ref().size() == exchange::BinaryProtocol::BUFFER_SIZE && 
			"[Retransmit Client] handle_request: received payload size doesn't match BinaryProtocol::BUFFER_SIZE");
		assert(new_message.header().sequence_number == request.first_packet && 
			"[Retransmit Client] handle_request: received payload sequence ID doesn't match requested sequence ID");

		result = write_market_data_to_reorder_buffer(
			reorder_buffer_, 
			new_message.get_payload_ref(), 
			new_message.header().sequence_number
		);
		if(!result) return result;
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) {
			logging::write<config::LOGGING>("Retransmit Client", "Received packet {}.", new_message.header().sequence_number);
		}
		else if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) {
			static size_t packet_count = 0;
			if(++packet_count % logging::MINIMAL_INTERVAL == 0) logging::write<config::LOGGING>("Retransmit Client", "Received {} retransmitted packets.", packet_count);
		}
	}
	return {};
}


std::expected<void, Error> RetransmitListener::run(std::stop_token stop_token) {
	while(!stop_token.stop_requested()) {
		auto request = request_buffer_.wait_pop(stop_token);
		if(!request) [[unlikely]] return {};
		auto result = handle_request(*request, stop_token);
		if(!result) [[unlikely]] return result;
	}
	return {};
}

std::expected<void, Error> RetransmitListener::start(std::stop_token stop_token) {
	if(socket_fd_ == INVALID) {
		auto result = initialize_and_connect();
		if(!result) {
			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("Retransmit Client", "Error: {}", result.error());
			return result;
		}
	}
	return run(stop_token);
}

RetransmitListener::~RetransmitListener() {
	if(socket_fd_ >= 0) {
		close(socket_fd_);
	}
}

}
