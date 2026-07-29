#include <print>
#include "book-state.hpp"
#include <string>
#include <sstream>
#include <cstdio>
#include <fstream>

constexpr void print_bbo(const BookState& order_book) {
	BookState::OrderSnapshot bbo = order_book.snapshot();
	std::string best_bid_price      = (bbo.best_bid.price == BookState::NO_BID)        ? "None" : std::to_string(bbo.best_bid.price);
    std::string best_offer_price    = (bbo.best_offer.price == BookState::NO_OFFER)    ? "None" : std::to_string(bbo.best_offer.price);
    
	std::println(
		"Best Bid \t Price {} \t Quantity {}\n"
		"Best Offer \t Price {} \t Quantity {}\n",
		best_bid_price, bbo.best_bid.quantity, best_offer_price, bbo.best_offer.quantity
	);
	std::fflush(stdout);
}

/*
Queries in the form:
ADD [orderID] [Price] [Quantity] [Side]
CANCEL [orderID]
TRADE [orderID] [Quantity]
*/

void log_update(const BookState::Update& update, const BookState& order_book) {
	static size_t action_counter = 0;

	std::println("Order Book Action # {}", ++action_counter);
	std::fflush(stdout);
	switch(update) {
		using enum BookState::Update;

		case NoChange:
			break;
		case NewBestBid:
			std::println(
				"New Best Bid\n "
					"----------------\n"
			);
			std::fflush(stdout);
			print_bbo(order_book);
			break;
		case NewBestOffer:
			std::println(
				"New Best Offer\n "
					"----------------\n"
			);
			std::fflush(stdout);
			print_bbo(order_book);
			break;
	}
}


bool parse_and_execute(const std::string& string_to_parse, BookState& order_book) {
	static const std::unordered_map<std::string, MarketEvent::Type> registry {
		{"ADD", MarketEvent::Type::Add},
		{"CANCEL", MarketEvent::Type::Cancel},
		{"TRADE", MarketEvent::Type::Trade},
	};

    std::stringstream parse(string_to_parse);
    std::string verb;
    MarketEvent new_event;
    parse >> verb >> new_event.order.order_id;
	
	if(!registry.contains(verb)) {
		std::println(stderr, "[Parser] Verb {} unknown.", verb);
		std::fflush(stderr);
        return false;
	}

	new_event.type = registry.at(verb);

	switch(new_event.type) {
		case MarketEvent::Type::Add:
			char side;
			parse >> new_event.order.price >> new_event.order.quantity >> side;

			static const std::unordered_map<char, Order::Side> side_map {
				{'B', Order::Side::Buy},
				{'S', Order::Side::Sell},
			};

			if(!side_map.contains(side)) {
				std::println(stderr, "[Parser] Side {} unknown.", verb);
				std::fflush(stderr);
				return false;
			}

			new_event.order.side = side_map.at(side);
			new_event.type = MarketEvent::Type::Add;
			break;
		case MarketEvent::Type::Cancel:
			new_event.type = MarketEvent::Type::Cancel;
			break;
		case MarketEvent::Type::Trade:
			parse >> new_event.order.quantity;
        	new_event.type = MarketEvent::Type::Trade;
			break;
	}

	BookState::Update update = order_book.execute(new_event);
	log_update(update, order_book);

    return true;
}

int main() {
    constexpr const char* file_name = "./test/order-book-queries.txt";
    std::ifstream ifs{file_name};

    BookState order_book;

    std::string line;
    std::getline(ifs, line); // Discard headers

    while(std::getline(ifs, line)) {
        parse_and_execute(line, order_book);
    }
    return 0;
}
