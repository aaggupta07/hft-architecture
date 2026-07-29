#include "exchange-simulator.hpp"

#include <print>

int main() {
	std::println("Starting exchange simulator.");

	exchange::ExchangeSimulator simulator;
	simulator.start();
}
