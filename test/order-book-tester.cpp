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
}

/*
Queries in the form:
ADD [orderID] [Price] [Quantity] [Side]
CANCEL [orderID]
TRADE [orderID] [Quantity]
*/

void log_update(const BookState::BookUpdate& update, const BookState& order_book) {
	static size_t action_counter = 0;

	std::println("Order Book Action # {}", ++action_counter);
	switch(update) {
		using enum BookState::BookUpdate;

		case NoChange:
			break;
		case NewBestBid:
			std::println(
				"New Best Bid\n "
					"----------------\n"
			);
			print_bbo(order_book);
			break;
		case NewBestOffer:
			std::println(
				"New Best Offer\n "
					"----------------\n"
			);
			print_bbo(order_book);
			break;
	}
}


bool parse_and_execute(const std::string& string_to_parse, BookState& order_book) {
	static const std::unordered_map<std::string, Order::Type> registry {
		{"ADD", Order::Type::Add},
		{"CANCEL", Order::Type::Cancel},
		{"TRADE", Order::Type::Trade},
	};

    std::stringstream parse(string_to_parse);
    std::string verb;
    Order new_order;
    parse >> verb >> new_order.order_id;
	
	if(!registry.contains(verb)) {
		std::println(stderr, "[Parser] Verb {} unknown.", verb);
        return false;
	}

	new_order.type = registry.at(verb);

	switch(new_order.type) {
		case Order::Type::Add:
			char side;
			parse >> new_order.price >> new_order.quantity >> side;

			static const std::unordered_map<char, Order::Side> side_map {
				{'B', Order::Side::Buy},
				{'S', Order::Side::Sell},
			};

			if(!side_map.contains(side)) {
				std::println(stderr, "[Parser] Side {} unknown.", verb);
				return false;
			}

			new_order.side = side_map.at(side);
			new_order.type = Order::Type::Add;
			break;
		case Order::Type::Cancel:
			new_order.type = Order::Type::Cancel;
			break;
		case Order::Type::Trade:
			parse >> new_order.quantity;
        	new_order.type = Order::Type::Trade;
			break;
	}

	BookState::BookUpdate update = order_book.execute(new_order);
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