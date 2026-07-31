#ifndef TCP_CONNECTION_HPP
#define TCP_CONNECTION_HPP

#include "protocol.hpp"
#include "exchange-errors.hpp"
#include "network-utils.hpp"

#include <array>
#include <expected>
#include <algorithm>
#include <cassert>
#include <span>

#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

namespace exchange {
template<size_t N>
class Connection {
public:
	enum class Status {
		Clear,
		SavedMessage,
		PartialSend,
		PartialReceive,
	};

private:
	using SocketFD = int;
	std::array<std::byte, N> buffer_;
	SocketFD socket_fd_ = -1;
	size_t head_ = 0;
	size_t tail_ = 0;
	Status connection_status_ = Status::Clear;

public:
	Connection() = default;
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;

	void steal(Connection&& other) noexcept {
		buffer_ = other.buffer_;
		socket_fd_ = other.socket_fd_;
		head_ = other.head_;
		tail_ = other.tail_;
		connection_status_ = other.connection_status_;

		other.socket_fd_ = -1;
		other.clear();
	}

	Connection(Connection&& other) noexcept {
		steal(std::move(other));
	}
	Connection& operator=(Connection&& other) noexcept {
		if(this != &other) {
			close();
			steal(std::move(other));
		}
		return *this;
	}

	~Connection() noexcept {
		close();
	}

	constexpr void set_socket(SocketFD socket) noexcept { socket_fd_ = socket; }
	constexpr Status status() const noexcept { return connection_status_; }
	constexpr SocketFD socket() const noexcept { return socket_fd_; }

	constexpr void clear() noexcept {
		head_ = 0;
		tail_ = 0;
		connection_status_ = Status::Clear;
	}

	void close() noexcept {
		clear();
		if(socket_fd_ >= 0) {
			::close(socket_fd_);
			socket_fd_ = -1;
		}
	}

	constexpr void save(const std::span<const std::byte> bytes) {
		assert(connection_status_ == Status::Clear || connection_status_ == Status::SavedMessage);
		assert(bytes.size() <= buffer_.size() - head_);
		std::ranges::copy(bytes, buffer_.data() + head_);
		head_ += bytes.size();
		connection_status_ = Status::SavedMessage;
	}

	std::expected<void, Error> send() {
		assert(socket_fd_ >= 0);
		assert(connection_status_ == Status::SavedMessage || connection_status_ == Status::PartialSend);
		
		connection_status_ = Status::PartialSend;

		if(network::send_range(socket_fd_, buffer_, tail_, head_)) {
			clear();
			return {};
		}

		if(errno == EAGAIN || errno == EWOULDBLOCK) {
				return std::unexpected(Error::WouldBlock);
			}
		if(errno == ECONNRESET) {
			return std::unexpected(Error::ClientConnectionClosed);
		}
		return std::unexpected(Error::SendToClient);
	}

	// Ignores `bytes` if a message was partially received - in this case, receive() 
	// will attempt to fetch the remainder of the partially received message instead.
	std::expected<std::span<std::byte>, Error> receive(size_t bytes) {
		assert(socket_fd_ >= 0);
		assert((connection_status_ == Status::Clear && bytes <= buffer_.size()) || connection_status_ == Status::PartialReceive);

		if(connection_status_ == Status::Clear) {
			head_ = bytes;
			connection_status_ = Status::PartialReceive;
		} 

		network::ReceiveStatus status = network::recv_range(socket_fd_, buffer_, tail_, head_);
		switch(status) {
			case network::ReceiveStatus::Success:
				{
					const size_t completed_size = head_;
					clear();
					return std::span{buffer_}.first(completed_size);
				}
			case network::ReceiveStatus::ConnectionClosed:
				return std::unexpected(Error::ClientConnectionClosed);
			case network::ReceiveStatus::Error:
				if(errno == EAGAIN || errno == EWOULDBLOCK) return std::unexpected(Error::WouldBlock);
				return std::unexpected(Error::ReceiveFromClient);
		}
		assert(false && "Connection (receive): unreachable");
		std::unreachable();
	}
};
}

#endif
