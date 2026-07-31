#ifndef MARKET_DATA_HANDLER_HPP
#define MARKET_DATA_HANDLER_HPP

#include "handler.hpp"
#include "binary-protocol.hpp"
#include "gap-detector.hpp"
#include "udp-listener.hpp"
#include "tcp-client.hpp"
#include "publisher.hpp"

#include <stop_token>
#include <string_view>

namespace handler {
class MarketDataHandler {
private:
	PacketBuffer udp_packet_buffer_;
	GapDetector gap_detector_;
	RetransmitRequestBuffer retransmit_request_buffer_;
	MarketReorderBuffer reorder_buffer_;

	RealTimeListener udp_feed_listener_;
	RetransmitListener retransmit_client_;
	MarketEventPublisher<exchange::BinaryProtocol> market_event_publisher_;

	std::stop_source stop_source_;


	void launch_real_time_listener	(std::stop_token stop_token);
	void launch_market_publisher	(std::stop_token stop_token);
	void launch_retransmit_client	(std::stop_token stop_token);
	void run						(std::stop_token stop_token);
public:
	MarketDataHandler(MarketEventBuffer& market_event_buffer);
	~MarketDataHandler() = default;

	MarketDataHandler(const MarketDataHandler&) = delete;
	MarketDataHandler& operator=(const MarketDataHandler&) = delete;
	MarketDataHandler(MarketDataHandler&&) = delete;
	MarketDataHandler& operator=(MarketDataHandler&&) = delete;

	void start(std::stop_token external_stop_token = {});
	void request_stop() noexcept { stop_source_.request_stop(); }
};
}



#endif
