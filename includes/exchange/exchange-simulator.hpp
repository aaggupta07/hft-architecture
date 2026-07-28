#ifndef EXCHANGE_SIMULATOR_HPP
#define EXCHANGE_SIMULATOR_HPP

#include "exchange-errors.hpp"
#include "protocol.hpp"

#include "order-generator.hpp"
#include "clob.hpp"
#include "ring-buffer.hpp"

#include "sequencer.hpp"

#include "circular-cache.hpp"
#include "broadcaster.hpp"
#include "tcp-retransmit.hpp"


class exchange::ExchangeSimulator {
private:

public:
	ExchangeSimulator() = default;
	~ExchangeSimulator() = default;

	ExchangeSimulator(const ExchangeSimulator&) = delete;
	ExchangeSimulator& operator=(const ExchangeSimulator&) = delete;
	ExchangeSimulator(ExchangeSimulator&&) = delete;
	ExchangeSimulator& operator=(ExchangeSimulator&&) = delete;

	void launch();
};

#endif