#include "exchange-simulator.hpp"
#include "market-data-handler.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <print>
#include <spawn.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;

namespace {
constexpr char EXCHANGE_LOG_PATH[] = "/private/tmp/hft-exchange.log";

void run_exchange_child() {
	const int log_fd = open(EXCHANGE_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if(log_fd == -1) {
		std::perror("open exchange log");
		_exit(EXIT_FAILURE);
	}
	if(dup2(log_fd, STDOUT_FILENO) == -1 || dup2(log_fd, STDERR_FILENO) == -1) {
		std::perror("redirect exchange output");
		_exit(EXIT_FAILURE);
	}
	close(log_fd);

	exchange::ExchangeSimulator simulator;
	simulator.start();
	_exit(EXIT_SUCCESS);
}

void open_exchange_terminal() {
	char script[] = "tell application \"Terminal\" to do script \"tail -f /private/tmp/hft-exchange.log\"";
	char program[] = "osascript";
	char option[] = "-e";
	char* const arguments[] = {program, option, script, nullptr};

	pid_t terminal_pid = -1;
	const int status = posix_spawnp(&terminal_pid, program, nullptr, nullptr, arguments, environ);
	if(status != 0) {
		std::println(stderr, "Could not open Terminal for exchange output: {}", std::strerror(status));
	}
}

template<typename Buffer>
void drain_market_events(std::stop_token stop_token, Buffer& buffer) {
	while(buffer.wait_get_tail_ref(stop_token) != nullptr) {
		buffer.consume();
	}
}
}

int main() {
	using MarketEventBuffer = LazyRingBuffer<MarketEvent, handler::config::MARKET_EVENT_BUFFER_SIZE>;

	const pid_t exchange_pid = fork();
	if(exchange_pid == -1) {
		std::println(stderr, "fork failed: {}", std::strerror(errno));
		return EXIT_FAILURE;
	}
	if(exchange_pid == 0) {
		run_exchange_child();
	}

	open_exchange_terminal();
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	MarketEventBuffer market_event_buffer;
	handler::MarketDataHandler market_data_handler(market_event_buffer);
	std::jthread event_drainer(drain_market_events<MarketEventBuffer>, std::ref(market_event_buffer));

	std::println("Starting market-data handler; exchange output is in a separate Terminal window.");
	std::fflush(stdout);
	market_data_handler.start();

	if(kill(exchange_pid, SIGTERM) == -1 && errno != ESRCH) {
		std::println(stderr, "Could not stop exchange child: {}", std::strerror(errno));
	}
	std::ignore = waitpid(exchange_pid, nullptr, 0);
	return EXIT_SUCCESS;
}
