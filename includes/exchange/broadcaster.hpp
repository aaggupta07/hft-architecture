#ifndef BROADCASTER_HPP
#define BROADCASTER_HPP

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <expected>
#include <span>

#include "exchange-errors.hpp"
#include "protocol.hpp"

class exchange::Broadcaster {
private:
    using SocketFD = int;
    using Port = size_t;

    SocketFD socket_fd_;
    Port port;

public:
    Port start();
    std::expected<void, Error> send(std::span<const Message::Byte> message_bytes);    
};


#endif