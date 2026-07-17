#include "order-book.hpp"
#include <print>

/*
For now, these simply print to std::cout
Later, will write to shared memory region via SeqLock + std::atomic
*/
void OrderBook::publish_new_best_bid() {
	std::println(
		"New Best Bid\n"
		"-------------"
		"\t Best Bid \t Price {} \t Quantity {}\n"
		"\t Best Offer \t Price {} \t Quantity {}\n",
		best_bid.price, best_bid.quantity, best_offer.price, best_offer.quantity
	);
}

void OrderBook::publish_new_best_offer() {
	std::println(	
		"New Best Offer\n"
		"-------------"
		"\t Best Bid \t Price {} \t Quantity {}\n"
		"\t Best Offer \t Price {} \t Quantity {}\n",
		best_bid.price, best_bid.quantity, best_offer.price, best_offer.quantity
	);
}

void OrderBook::update_best_bid_add(Order& order) {
    if(order.price > best_bid.price) {
        best_bid.price = order.price;
        best_bid.quantity = order.quantity;
        publish_new_best_bid();
    }
    else if(order.price == best_bid.price) {
        best_bid.quantity += order.quantity;
        publish_new_best_bid();
    }
}

void OrderBook::update_best_offer_add(Order& order) {
    if(order.price < best_offer.price) {
        best_offer.price = order.price;
        best_offer.quantity = order.quantity;
        publish_new_best_offer();
    }
    else if(order.price == best_offer.price) {
        best_offer.quantity += order.quantity;
        publish_new_best_offer();
    }
}

void OrderBook::update_bbo_add(Order& order) {
    if(order.side == Order::Side::Buy) {
        update_best_bid_add(order);
    }
    else {
        update_best_offer_add(order);
    }
}

void OrderBook::update_best_bid_reduce(Order& order) {
    if(order.price == best_bid.price && order.quantity == best_bid.quantity) {
        best_bid.price = (!bids.empty()) ? *bids.begin() : NO_BID_OR_OFFER;
        best_bid.quantity = (!bids.empty()) ? price_levels.find(best_bid.price)->get().total_quantity : 0;
        publish_new_best_offer();
    }
    else if(order.price == best_bid.price) {
        best_bid.quantity -= order.quantity;
        publish_new_best_offer();
    }
}

void OrderBook::update_best_offer_reduce(Order& order) {
    if(order.price == best_offer.price && order.quantity == best_offer.quantity) {
        best_offer.price = (!bids.empty()) ? *offers.begin() : NO_BID_OR_OFFER;
        best_offer.quantity = (!bids.empty()) ? price_levels.find(best_offer.price)->get().total_quantity : 0;
        publish_new_best_offer();
    }
    else if(order.price == best_bid.price) {
        best_bid.quantity -= order.quantity;
        publish_new_best_offer();
    }
}

void OrderBook::update_bbo_reduce(Order& order) {
    if(order.side == Order::Side::Buy) {
        update_best_bid_reduce(order);
    }
    else {
        update_best_offer_reduce(order);
    }
}

void OrderBook::add_bid_or_offer(Order& order) {
    if(order.side == Order::Side::Buy) {
        bids.insert(order.price);
    }
    else {
        offers.insert(order.price);
    }
}

void OrderBook::remove_bid_or_offer(Order& order) {
    if(order.side == Order::Side::Buy) {
        bids.erase(order.price);
    }
    else {
        offers.erase(order.price);
    }
}


/*
- Allocate a new order from OrderPool
- Insert the order into the order_map
- Check if price level exists
    - If it exists, insert it into the price level, update total quantity
    - If it doesn't exist, create a new price level and insert into the map + prices set
    - If price improves best_price, update best_price
*/
void OrderBook::add(Order order) {
    Index order_index = order_pool.allocate(order);
    order_map.add(order.order_id, order_index);

    auto result = price_levels.find_then_add(order.price);
    if(!result) {
        price_levels.add_on_saved_index(order.price, PriceLevel {
            .head = order_index,
            .tail = order_index,
            .total_quantity = order.quantity,
        });

        add_bid_or_offer(order);
    }
    else {
        PriceLevel& price_level = *result;
        price_level.total_quantity += order.quantity;
        order_pool.get(price_level.tail).next = order_index;
        order_pool.get(order_index).previous = price_level.tail;
        price_level.tail = order_index;
    }

    update_bbo_add(order);
}

void OrderBook::detach(const Order& order, PriceLevel& price_level) {
    if(order.previous != Order::NULL_INDEX) {
		order_pool.get(order.previous).next = order.next;
	}
    else price_level.head = order.next;			// Was head
    if(order.next != Order::NULL_INDEX) {
		order_pool.get(order.next).previous = order.previous;
	}
    else price_level.tail = order.previous;		// Was tail
}

/*
- Find the order_index via the order map
- Check the price level and reduce order.quantity - erase the price level if it hits 0
- Erase the order from the order_map and deallocate it from the order pool
- Relink the order intrusive linked list
*/
void OrderBook::purge_order(OrderID resting_order_id) {
    Index order_index = order_map.find_then_remove(resting_order_id)->get();
    Order& order = order_pool.get(order_index);

    PriceLevel& price_level = price_levels.find_then_remove(order.price)->get();
    price_level.total_quantity -= order.quantity;
    if(price_level.total_quantity == 0) {
    	price_levels.remove_on_saved_index();
        remove_bid_or_offer(order);
    }
    else {
        detach(order, price_level);
    }

	order_map.remove_on_saved_index();
	update_bbo_reduce(order);			// Publish the new BBO if changed
	order_pool.free(order_index);
}

void OrderBook::cancel(OrderID resting_order_id) {
   purge_order(resting_order_id);
}

/*
- Find the order_index via the order map
- Check the price level and reduce quantity - erase the price level if it hits 0
- Erase the order from the order_map and deallocate it from the order pool if its quantity hit 0
*/
void OrderBook::trade(OrderID resting_order_id, Quantity quantity) {
    Index order_index = order_map.find_then_remove(resting_order_id)->get();
    Order& order = order_pool.get(order_index);
    if(order.quantity == quantity) {
        purge_order(resting_order_id);
    }
    else {
        PriceLevel& price_level = price_levels.find(order.price)->get();
        order.quantity -= quantity;
        price_level.total_quantity -= quantity;
		update_bbo_reduce(order);
    }
}

void OrderBook::cancel(Order order) {
    cancel(order.order_id);
}

void OrderBook::trade(Order order) {
    trade(order.order_id, order.quantity);
}