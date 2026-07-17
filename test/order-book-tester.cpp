#include <print>
#include "order-book.hpp"
#include <string>
#include <sstream>
#include <cstdio>
#include <fstream>

/*
Queries in the form:
ADD [orderID] [Price] [Quantity] [Side]
CANCEL [orderID]
TRADE [orderID] [Quantity]
*/
bool parse_and_execute(const std::string& string_to_parse, OrderBook& order_book) {
    std::stringstream parse(string_to_parse);
    std::string verb;
    Order new_order;
    parse >> verb >> new_order.order_id;

    if(verb == "ADD") {
        char side;
        new_order.order_id = new_order.order_id;
        parse >> new_order.price >> new_order.quantity >> side;

        if(side == 'B') new_order.side = Order::Side::Buy;
        else if(side == 'S') new_order.side = Order::Side::Sell;
        else {
            std::println(stderr, "[Parser] Side {} unknown.", verb);
            return false;
        }

        order_book.add(new_order);
    }
    else if(verb == "CANCEL") {
        order_book.cancel(new_order);
    }
    else if(verb == "TRADE") {
        parse >> new_order.quantity;
        order_book.trade(new_order);
    }
    else {
        std::println(stderr, "[Parser] Verb {} unknown.", verb);
        return false;
    }
    return true;
}

int main() {
    constexpr const char* file_name = "./test/order-book-queries.txt";
    std::ifstream ifs{file_name};

    OrderBook order_book;

    std::string line;
    std::getline(ifs, line); // Discard headers

    size_t counter = 0;
    while(std::getline(ifs, line)) {
        std::println("Order Book Action # {}", ++counter);
        parse_and_execute(line, order_book);

    }
    return 0;
}