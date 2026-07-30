#include "udp-listener.hpp"
#include "network-utils.hpp"
#include "config.hpp"

#include <print>
#include <cerrno>

namespace handler {
std::expected<void, Error> RealTimeListener::initialize() {
	socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
	if(socket_fd_ == INVALID) return std::unexpected(Error::StartUDPListener);

	if(	!network::enable_udp_port_sharing(socket_fd_) || 
		!network::resize_recv_buffer(socket_fd_, config::LISTENER_SOCKET_BUFFER_SIZE)) 
	{
		return std::unexpected(Error::StartUDPListener);
	}

	local.sin_family		= AF_INET;
	local.sin_port			= htons(exchange::config::MCAST_PORT);
	local.sin_addr.s_addr	= INADDR_ANY;
	if(bind(socket_fd_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == -1) {
		return std::unexpected(Error::StartUDPListener);
	}

	network::Status status = network::join_multicast_group(socket_fd_, exchange::config::MCAST_GROUP);
	switch(status) {
		case network::Status::InvalidIPAddress:
			return std::unexpected(Error::InvalidMulticastGroup);
		case network::Status::ProcedureError:
			return std::unexpected(Error::StartUDPListener);
		case network::Status::Good:
			if constexpr(config::LOGGING) {
				std::println("[Real Time Feed Listener] Initialized.");
			}
			return {};
	}
}


std::expected<void, Error> RealTimeListener::run(std::stop_token stop_token) {
	while(!stop_token.stop_requested()) {
		auto new_message = buffer_.wait_get_head_ref(stop_token);
		if(!new_message) [[unlikely]] return {};

		int status = recv(socket_fd_, &(new_message->get_buffer_ref()), exchange::EncodedMessage::MAX_WIRE_SIZE, 0);
		if(status == INVALID) [[unlikely]] return std::unexpected(Error::UDPListen);
		else if(stop_token.stop_requested()) [[unlikely]] return {};

		buffer_.publish();
	}
	return {};
}

std::expected<void, Error> RealTimeListener::start(std::stop_token stop_token) {
	if(socket_fd_ == INVALID) {
		auto result = initialize();
		if(!result) return std::unexpected(result.error());
	}

	return run(stop_token);
}

};