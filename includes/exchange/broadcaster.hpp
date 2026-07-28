#ifndef BROADCASTER_HPP
#define BROADCASTER_HPP

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <expected>

#include "protocol.hpp"
#include "exchange-errors.hpp"

namespace exchange {
class Broadcaster {
private:
    using SocketFD = int;
    using Port = uint16_t;

    SocketFD socket_fd_ = 0;
    Port port = 0;

public:
    Broadcaster() = default;
    std::expected<void, Error> start();
    std::expected<void, Error> send(const std::span<const std::byte> message);    
};
}

#endif