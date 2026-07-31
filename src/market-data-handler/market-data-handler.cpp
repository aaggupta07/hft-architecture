#include "market-data-handler.hpp"
#include "log.hpp"

#include <thread>

namespace handler {
MarketDataHandler::MarketDataHandler(MarketEventBuffer& market_event_buffer)
	: reorder_buffer_(exchange::config::FIRST_SEQUENCE_ID),
	udp_feed_listener_	(udp_packet_buffer_),
	retransmit_client_		(retransmit_request_buffer_, reorder_buffer_),
	market_event_publisher_	(reorder_buffer_, market_event_buffer) {}

void MarketDataHandler::launch_real_time_listener(std::stop_token stop_token) {
	auto result = udp_feed_listener_.start(stop_token);
	if(!result) [[unlikely]] {
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("MarketDataHandler", "Error: {}", result.error());
		request_stop();
	}
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("MarketDataHandler", "Real-time listener stopped.");
}

void MarketDataHandler::launch_market_publisher(std::stop_token stop_token) {
	market_event_publisher_.run(stop_token);
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("MarketDataHandler", "Market event publisher stopped.");
	request_stop();
}

void MarketDataHandler::launch_retransmit_client(std::stop_token stop_token) {
	auto result = retransmit_client_.start(stop_token);
	if(!result) [[unlikely]] {
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("MarketDataHandler", "Error: {}", result.error());
		request_stop();
	}
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("MarketDataHandler", "Retransmit client stopped.");
}

void MarketDataHandler::run(std::stop_token stop_token) {
	while(!stop_token.stop_requested()) {
		exchange::EncodedMessage* new_packet = udp_packet_buffer_.wait_get_tail_ref(stop_token);
		if(!new_packet) {
			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("MarketDataHandler", "Stopped while waiting for a market event.");
			return;
		}
		// Over the real-time feed broadcast, all packets should contain real data and no error messages
		new_packet->decode_header_from_buffer();

		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) logging::write<config::LOGGING>("MarketDataHandler", "Pulled new UDP packet from ring buffer.");

		auto request = gap_detector_.has_gap(new_packet->header());
		if(request) {
			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) logging::write<config::LOGGING>("MarketDataHandler", "Gap detected, packets {} to {}", request->first_packet, request->last_packet);
			if(!retransmit_request_buffer_.try_push(*request)) [[unlikely]] {
				// Retransmit request buffer is full
				if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("MarketDataHandler", "Error: {}", Error::RetransmitRequestBufferFull);
				request_stop();
				return;
			}
		}

		// Write packet data to the reordering buffer
		auto result = write_market_data_to_reorder_buffer(
			reorder_buffer_, 
			new_packet->get_payload_ref(), 
			new_packet->header().sequence_number
		);

		// Release the slot
		udp_packet_buffer_.consume();

		// Other producer or consumer is too far behind on the reorder buffer
		if(!result) [[unlikely]] {
			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("MarketDataHandler", "Error: {}", result.error());
			request_stop();
			return;
		}

		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) {
			logging::write<config::LOGGING>("MarketDataHandler", "Submitted packet data to reorder buffer.");
		}
		else if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) {
			static size_t packet_count = 0;
			if(++packet_count % logging::MINIMAL_INTERVAL == 0) logging::write<config::LOGGING>("MarketDataHandler", "Submitted {} packets to the reorder buffer.", packet_count);
		}
	}
}

void MarketDataHandler::start(std::stop_token external_stop_token) {
	// Launch in this specific order: Retransmit Client, Market Publisher, Real-Time Listener
	std::stop_callback callback(external_stop_token, [this] { request_stop(); });
	std::stop_token stop_token = stop_source_.get_token();

	std::jthread retransmit_client_thread(&MarketDataHandler::launch_retransmit_client, this, stop_token);
	std::jthread market_publisher_thread(&MarketDataHandler::launch_market_publisher, this, stop_token);
	std::jthread real_time_listener_thread(&MarketDataHandler::launch_real_time_listener, this, stop_token);
	run(stop_token);
	request_stop();
}

}
