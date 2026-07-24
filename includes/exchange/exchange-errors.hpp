#ifndef EXCHANGE_ERRORS_HPP
#define EXCHANGE_ERRORS_HPP

namespace exchange {
enum class Error {
    OrderGenerate,
    StartBroadcast,
    Send,
    RetransmitCacheFull,
};
}

#endif