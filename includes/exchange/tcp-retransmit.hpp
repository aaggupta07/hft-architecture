#ifndef TCP_RETRANSMIT_HPP
#define TCP_RETRANSMIT_HPP

#include "protocol.hpp"
#include "circular-cache.hpp"
#include "encoded-message.hpp"
#include "exchange-errors.hpp"
#include "tcp-connection.hpp"
#include "config.hpp"

#include <expected>
#include <span>
#include <vector>
#include <optional>
#include <stop_token>

namespace exchange{
struct RetransmitRequest {
	SequenceID first_packet;
	SequenceID last_packet;

	static RetransmitRequest 	parse(const std::span<std::byte> wire);
	static auto 				serialize(const RetransmitRequest& request);
};

class RetransmitServer {
private:
	static constexpr int INVALID 	= -1;
	static constexpr size_t N		= EncodedMessage::MAX_WIRE_SIZE;

	

	using Cache = CircularCache<EncodedMessage, config::RETRANSMIT_CACHE_SIZE>;
	using SocketFD = int;
	using EventQueue = int;
	
	struct SavedConnection {
		Connection<N> connection;
		std::optional<RetransmitRequest> request = std::nullopt;
	};

	SocketFD 	socket_fd_ 			= INVALID;
	EventQueue 	event_queue_		= INVALID;
	size_t 		current_connections_ = 0;

	Cache& retransmit_cache_;
	std::vector<SavedConnection> connection_buffers;

	static 	std::expected<void, Error> 		set_socket_nonblocking	(int socket_fd);
	static 	std::expected<void, Error> 		register_read_event		(int kq, int socket_fd);
	static 	std::expected<void, Error> 		register_write_event	(int kq, int socket_fd);
	static 	void					 		unregister				(int kq, int socket_fd) noexcept;

	static 	std::expected<SocketFD, Error> 	get_listener			();

			std::expected<SocketFD, Error> 	get_connected_socket	();
			std::expected<void, Error> 		handle_new_connections	();
			std::expected<void, Error> 		stream_packets			(SavedConnection& client);
			std::expected<void, Error> 		receive_request			(SavedConnection& client);
			std::expected<void, Error> 		handle_request			(SocketFD connected_socket);
			std::expected<void, Error> 		run_event_loop			(std::stop_token stop_token);

			void 							notify_and_close		(SavedConnection& client, const Error& error) noexcept; // best effort
			void							close_client			(SavedConnection& client) noexcept;
			void 							close_server			() noexcept;

	
	constexpr void log(const Error& error) const;

public:
	explicit constexpr RetransmitServer(Cache& retransmit_cache)
		: retransmit_cache_(retransmit_cache), connection_buffers(config::DEFAULT_CONNECTION_BUFFERS) {}

	std::expected<void, Error> initialize();
	std::expected<void, Error> start(std::stop_token stop_token);
};



}

#endif
