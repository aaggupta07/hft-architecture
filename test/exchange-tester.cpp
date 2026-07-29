#include "exchange-simulator.hpp"

#include <cstdio>
#include <print>

int main() {
	std::println("Starting exchange simulator.");
	std::fflush(stdout);

	exchange::ExchangeSimulator simulator;
	simulator.start();

	std::println("Exchange simulation complete.");
	std::fflush(stdout);
}
