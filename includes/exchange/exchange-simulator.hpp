#ifndef EXCHANGE_SIMULATOR_HPP
#define EXCHANGE_SIMULATOR_HPP

#include "exchange-errors.hpp"
#include "order-generator.hpp"
#include "broadcaster.hpp"
#include "sequencer.hpp"
#include "tcp-retransmit.hpp"
#include "ring-buffer.hpp"
#include "protocol.hpp"

class exchange::ExchangeSimulator {
private:
    static constexpr bool LOGGING = true;
};

#endif