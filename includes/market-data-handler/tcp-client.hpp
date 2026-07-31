#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include "encoded-message.hpp"
#include "handler-errors.hpp"
#include "tcp-retransmit.hpp"
#include "handler.hpp"

#include <expected>
#include <stop_token>

namespace handler {
class RetransmitListener {
private:
	using SocketFD = int;
	
	static constexpr int INVALID = -1;

	SocketFD socket_fd_ = INVALID;
	RetransmitRequestBuffer& request_buffer_;
	MarketReorderBuffer& reorder_buffer_;

	std::expected<void, Error> connect_to_server();
	std::expected<void, Error> initialize_and_connect();

	std::expected<void, Error> send_request_to_server(const exchange::RetransmitRequest& request);
	std::expected<void, Error> receive_and_decode_header(exchange::EncodedMessage& new_message);
	std::expected<void, Error> receive_payload(exchange::EncodedMessage& new_message);

	std::expected<void, Error> handle_request(exchange::RetransmitRequest& request, std::stop_token stop_token);

	std::expected<void, Error> run(std::stop_token stop_token);

	static constexpr void log_server_error_message(const exchange::MessageHeader& header);

public:
	RetransmitListener(RetransmitRequestBuffer& request_buffer, MarketReorderBuffer& reorder_buffer)
		: request_buffer_(request_buffer), reorder_buffer_(reorder_buffer) {}
	~RetransmitListener();

	std::expected<void, Error> start(std::stop_token stop_token);
};
}

#endif