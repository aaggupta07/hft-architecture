#ifndef TCP_CONNECTION_HPP
#define TCP_CONNECTION_HPP

#include "protocol.hpp"
#include "exchange-errors.hpp"

#include <array>
#include <expected>
#include <algorithm>
#include <cassert>

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
	std::array<Byte, N> buffer_;
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

		other.close();
	}
	Connection(Connection&& other) noexcept {
		steal(other);
	}
	Connection& operator=(Connection&& other) noexcept {
		if(this != &other) {
			close();
			steal(other);
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

	constexpr void save(const std::span<const Byte> bytes) {
		assert(connection_status_ == Status::Clear || connection_status_ == Status::SavedMessage);
		assert(bytes.size() < buffer_.size() - head_);
		std::ranges::copy(bytes, buffer_.data() + head_);
		head_ += bytes.size();
		connection_status_ = Status::SavedMessage;
	}

	std::expected<void, Error> send() {
		assert(socket_fd_ >= 0);
		assert(connection_status_ == Status::SavedMessage || connection_status_ == Status::PartialSend);
		
		connection_status_ = Status::PartialSend;

		int bytes_sent = 0;
		for(size_t total_bytes = tail_; total_bytes < head_; total_bytes += bytes_sent) {
			bytes_sent = send(socket_fd_, buffer_.data() + tail_, head_ - bytes_sent, 0);
			if(bytes_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
				return std::unexpected(Error::WouldBlock);
			}
			else if(bytes_sent == -1 && errno == ECONNRESET) {
				return std::unexpected(Error::ClientConnectionClosed);
			}
			else if(bytes_sent == -1) {
				return std::unexpected(Error::SendToClient);
			}
			tail_ += bytes_sent;
		}

		clear();
		return {};
	}

	std::expected<std::array<Byte, N>, Error> receive(size_t bytes) {
		assert(socket_fd_ >= 0);
		assert(head_ + bytes < buffer_.size());
		assert(connection_status_ == Status::Clear || connection_status_ == Status::PartialReceive);

		connection_status_ = Status::PartialReceive;

		int bytes_received = 0;
		for(size_t total_bytes = 0; total_bytes < bytes; total_bytes += bytes_received) {
			bytes_received = recv(socket_fd_, buffer_.data() + head_, bytes - total_bytes, 0);
			if(bytes_received == 0) {
				return std::unexpected(Error::ClientConnectionClosed);
			}
			else if(bytes_received == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
				return std::unexpected(Error::WouldBlock);
			}
			else if(bytes_received == -1) {
				return std::unexpected(Error::ReceiveFromClient);
			}
			head_ += bytes_received;
		}

		clear();
		return buffer_;
	}
};
}

#endif