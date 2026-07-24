#ifndef EXCHANGE_ERRORS_HPP
#define EXCHANGE_ERRORS_HPP

namespace exchange {
enum class Error {
    OrderGenerateError,
    SendError,
    RetransmitCacheFull,

};
}

#endif