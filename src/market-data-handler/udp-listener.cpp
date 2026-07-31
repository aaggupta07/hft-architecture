#include "udp-listener.hpp"
#include "network-utils.hpp"
#include "config.hpp"
#include "log.hpp"

#include <unistd.h>

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

	network::JoinStatus status = network::join_multicast_group(socket_fd_, exchange::config::MCAST_GROUP);
	switch(status) {
		case network::JoinStatus::InvalidIPAddress:
			return std::unexpected(Error::InvalidMulticastGroup);
		case network::JoinStatus::ProcedureError:
			return std::unexpected(Error::StartUDPListener);
		case network::JoinStatus::Success:
			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("Real Time Feed Listener", "Initialized.");
			return {};
	}
}


std::expected<void, Error> RealTimeListener::run(std::stop_token stop_token) {
	while(!stop_token.stop_requested()) {
		auto new_message = buffer_.try_get_head_ref();
		if(!new_message) [[unlikely]] return std::unexpected(Error::HandlerTooSlow);

		int status = recv(socket_fd_, new_message->get_buffer_ref().data(), exchange::EncodedMessage::MAX_WIRE_SIZE, 0);
		if(status == INVALID) [[unlikely]] return std::unexpected(Error::UDPListen);
		else if(stop_token.stop_requested()) [[unlikely]] return {};

		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) {
			logging::write<config::LOGGING>("Real Time Feed Listener", "Received packet.");
		}
		else if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) {
			static size_t packet_count = 0;
			if(++packet_count % logging::MINIMAL_INTERVAL == 0) logging::write<config::LOGGING>("Real Time Feed Listener", "Received {} packets.", packet_count);
		}

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

RealTimeListener::~RealTimeListener() {
	if(socket_fd_ != INVALID) {
		close(socket_fd_);
	}
}

};
