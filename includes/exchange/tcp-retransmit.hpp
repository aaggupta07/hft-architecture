#ifndef TCP_RETRANSMIT_HPP
#define TCP_RETRANSMIT_HPP

#include "protocol.hpp"
#include "circular-cache.hpp"
#include "encoded-message.hpp"
#include "exchange-errors.hpp"
#include "tcp-connection.hpp"

#include <expected>
#include <span>
#include <vector>
#include <optional>

namespace exchange{
struct RetransmitRequest {
	SequenceID first_packet;
	SequenceID last_packet;

	static RetransmitRequest 	parse(const std::span<Byte> wire);
	static auto 				serialize(const RetransmitRequest& request);
};

class RetransmitServer {
private:
	static constexpr size_t RETRANSMIT_CACHE_SIZE 		= 1 << 10;
	static constexpr const char* RETRANSMIT_PORT 		= "40000";
	static constexpr int MAX_PENDING_CONNECTIONS 		= 10;
	static constexpr bool LOGGING						= true;

	static constexpr int INVALID 						= -1;
	static constexpr size_t N							= EncodedMessage::MAX_WIRE_SIZE;

	static constexpr size_t DEFAULT_CONNECTION_BUFFERS 	= 5;
	static constexpr size_t MAX_CONNECTION_BUFFERS 		= 100;

	using Cache = CircularCache<EncodedMessage, RETRANSMIT_CACHE_SIZE>;
	using SocketFD = int;
	using EventQueue = int;
	
	struct SavedConnection {
		Connection<N> connection;
		std::optional<RetransmitRequest> request = std::nullopt;
	};

	SocketFD socket_fd_ 	= INVALID;
	EventQueue event_queue_	= INVALID;
	Cache& retransmit_cache_;
	std::vector<SavedConnection> connection_buffers;

	static 	std::expected<void, Error> 		set_socket_nonblocking	(int socket_fd);
	static 	std::expected<void, Error> 		register_read_event		(int kq, int socket_fd);
	static 	std::expected<void, Error> 		register_write_event	(int kq, int socket_fd);
	static 	void					 		unregister				(int kq, int socket_fd);

	static 	std::expected<SocketFD, Error> 	get_listener			();
			std::expected<SocketFD, Error> 	handle_new_connection	();
			std::expected<void, Error> 		stream_packets			(SavedConnection& client);
			std::expected<void, Error> 		receive_request			(SavedConnection& client);
			std::expected<void, Error> 		handle_request			(SocketFD connected_socket);
			std::expected<void, Error> 		run_event_loop			();
	
			void							close_client			(SavedConnection& client);

public:
	explicit constexpr RetransmitServer(Cache& retransmit_cache)
		: retransmit_cache_(retransmit_cache), connection_buffers(DEFAULT_CONNECTION_BUFFERS) {}

	std::expected<void, Error> initialize();
	std::expected<void, Error> start();
};



}

#endif