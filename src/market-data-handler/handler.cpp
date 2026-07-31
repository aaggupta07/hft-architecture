#include "handler.hpp"

namespace handler {
std::expected<void, Error> write_market_data_to_reorder_buffer(
	MarketReorderBuffer& reorder_buffer, SerializedMarketEventSpan event, SequenceID packet_id) {
	auto result = reorder_buffer.try_write_to(event.subspan<0, exchange::BinaryProtocol::BUFFER_SIZE>(), packet_id);
	if(result == MarketReorderBuffer::WriteStatus::LikelySlowReader) [[unlikely]] {
		return std::unexpected(Error::ReaderTooFarBehind);
	}
	if(result == MarketReorderBuffer::WriteStatus::LikelySlowWriter) [[unlikely]] {
		return std::unexpected(Error::WriterTooFarBehind);
	}

	return {};
}



}