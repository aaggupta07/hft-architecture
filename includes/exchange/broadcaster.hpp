#ifndef BROADCASTER_HPP
#define BROADCASTER_HPP

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <expected>
#include <span>

#include "exchange-errors.hpp"

namespace exchange {
class Broadcaster {
private:
    using SocketFD = int;
    using Port = uint16_t;
	static constexpr int INVALID = -1;

    SocketFD socket_fd_ = INVALID;

public:
    Broadcaster() = default;
    std::expected<void, Error> start();
    std::expected<void, Error> send(const std::span<const std::byte> message);
	~Broadcaster();
};
}

#endif