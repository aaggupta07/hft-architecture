#ifndef HANDLER_HPP
#define HANDLER_HPP

#include <cstdint>
#include <cstddef>

namespace data {
using SequenceID = uint64_t;

class MulticastListener;
class GapDetector;
class RetransmitClient;
class MarketEventPublisher;

class MarketDataHandler;

};

#endif