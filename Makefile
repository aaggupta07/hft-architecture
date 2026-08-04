CXX := clang++
STACK_SIZE = 0x1000000 # 16MB

INCLUDE_DIRS := includes includes/order-book includes/exchange includes/utils includes/market-data-handler includes/strategy-oms
CPPFLAGS := $(addprefix -I,$(INCLUDE_DIRS)) -MMD -MP
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -g
LDFLAGS := -Wl,-stack_size,0x1000000
RELEASE_FLAGS := -O3 -flto -DNDEBUG -march=native -fomit-frame-pointer

COMMON_SOURCE_DIRS := src/order-book src/utils
EXCHANGE_SOURCE_DIRS := $(COMMON_SOURCE_DIRS) src/exchange
MARKET_HANDLER_SOURCE_DIRS := $(COMMON_SOURCE_DIRS) src/market-data-handler

EXCHANGE_SOURCES := $(foreach dir,$(EXCHANGE_SOURCE_DIRS),$(wildcard $(dir)/*.cpp))
MARKET_HANDLER_SOURCES := $(foreach dir,$(MARKET_HANDLER_SOURCE_DIRS),$(wildcard $(dir)/*.cpp))
EXCHANGE_OBJECTS := $(patsubst src/%.cpp,obj/%.o,$(EXCHANGE_SOURCES))
MARKET_HANDLER_OBJECTS := $(patsubst src/%.cpp,obj/%.o,$(MARKET_HANDLER_SOURCES))

# The market-data handler consumes the exchange wire protocol and retransmit
# request codec, but does not need the simulator, CLOB, or broadcaster.
MARKET_HANDLER_EXCHANGE_SOURCES := \
	src/exchange/binary-protocol.cpp \
	src/exchange/encoded-message.cpp \
	src/exchange/protocol.cpp \
	src/exchange/tcp-retransmit.cpp
MARKET_HANDLER_EXCHANGE_OBJECTS := $(patsubst src/%.cpp,obj/%.o,$(MARKET_HANDLER_EXCHANGE_SOURCES))

TEST_SOURCES := test/order-book-tester.cpp test/exchange-tester.cpp test/exchange-pipeline-tester.cpp test/market-data-handler-tester.cpp test/exchange-market-data-tester.cpp
TEST_OBJECTS := $(patsubst test/%.cpp,obj/test/%.o,$(TEST_SOURCES))
OBJECTS := $(sort $(EXCHANGE_OBJECTS) $(MARKET_HANDLER_OBJECTS) $(MARKET_HANDLER_EXCHANGE_OBJECTS) $(TEST_OBJECTS))
DEPENDENCIES := $(OBJECTS:.o=.d)
INTEGRATION_OBJECTS := $(sort $(EXCHANGE_OBJECTS) $(MARKET_HANDLER_OBJECTS) $(MARKET_HANDLER_EXCHANGE_OBJECTS))

.PHONY: all clean test

all: bin/exchange

obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

obj/test/%.o: test/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

bin/order-book: obj/test/order-book-tester.o obj/order-book/book-state.o obj/order-book/order-pool.o obj/order-book/closed-hash-map.o
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

bin/exchange: obj/test/exchange-tester.o $(EXCHANGE_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

bin/market-handler: obj/test/market-data-handler-tester.o $(MARKET_HANDLER_OBJECTS) $(MARKET_HANDLER_EXCHANGE_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

bin/exchange-pipeline-test: obj/test/exchange-pipeline-tester.o $(EXCHANGE_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

bin/exchange-market-data: obj/test/exchange-market-data-tester.o $(INTEGRATION_OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

test: bin/exchange-pipeline-test
	bin/exchange-pipeline-test

clean:
	rm -rf bin obj

-include $(DEPENDENCIES)
