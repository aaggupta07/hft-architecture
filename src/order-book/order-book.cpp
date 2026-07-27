#include "order-book.hpp"

auto BookState::update_best_bid_add(const Order& order) -> BookUpdate {
    if(order.price > bbo.best_bid.price) {
        bbo.best_bid.price = order.price;
        bbo.best_bid.quantity = order.quantity;
        return BookUpdate::NewBestBid;
    }
    else if(order.price == bbo.best_bid.price) {
        bbo.best_bid.quantity += order.quantity;
        return BookUpdate::NewBestBid;
    }
	return BookUpdate::NoChange;
}

auto BookState::update_best_offer_add(const Order& order) -> BookUpdate {
    if(order.price < bbo.best_offer.price) {
        bbo.best_offer.price = order.price;
        bbo.best_offer.quantity = order.quantity;
        return BookUpdate::NewBestOffer;
    }
    else if(order.price == bbo.best_offer.price) {
        bbo.best_offer.quantity += order.quantity;
        return BookUpdate::NewBestOffer;
    }
	return BookUpdate::NoChange;
}

auto BookState::update_bbo_add(const Order& order) -> BookUpdate {
    if(order.side == Order::Side::Buy) {
        return update_best_bid_add(order);
    }
    else {
        return update_best_offer_add(order);
    }
}

auto BookState::update_best_bid_reduce(const Order& order) -> BookUpdate {
    if(order.price == bbo.best_bid.price && order.quantity == bbo.best_bid.quantity) {
        bbo.best_bid.price = (!bids.empty()) ? *bids.begin() : NO_BID;
        bbo.best_bid.quantity = (!bids.empty()) ? price_levels.find(bbo.best_bid.price)->get().total_quantity : 0;
        return BookUpdate::NewBestBid;
    }
    else if(order.price == bbo.best_bid.price) {
        bbo.best_bid.quantity -= order.quantity;
        return BookUpdate::NewBestBid;
    }
	return BookUpdate::NoChange;
}

auto BookState::update_best_offer_reduce(const Order& order) -> BookUpdate {
    if(order.price == bbo.best_offer.price && order.quantity == bbo.best_offer.quantity) {
        bbo.best_offer.price = (!offers.empty()) ? *offers.begin() : NO_OFFER;
        bbo.best_offer.quantity = (!offers.empty()) ? price_levels.find(bbo.best_offer.price)->get().total_quantity : 0;
        return BookUpdate::NewBestOffer;
    }
    else if(order.price == bbo.best_offer.price) {
        bbo.best_offer.quantity -= order.quantity;
        return BookUpdate::NewBestOffer;
    }
	return BookUpdate::NoChange;
}

auto BookState::update_bbo_reduce(const Order& order) -> BookUpdate {
    if(order.side == Order::Side::Buy) {
       return update_best_bid_reduce(order);
    }
    else {
       return update_best_offer_reduce(order);
    }
}

void BookState::add_bid_or_offer(const Order& order) {
    if(order.side == Order::Side::Buy) {
        bids.insert(order.price);
    }
    else {
        offers.insert(order.price);
    }
}

void BookState::remove_bid_or_offer(const Order& order) {
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
auto BookState::add(const Order& order) -> BookUpdate {
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

    return update_bbo_add(order);
}

void BookState::detach(const Order& order, PriceLevel& price_level) {
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
auto BookState::purge_order(OrderID resting_order_id) -> BookUpdate {
    Index order_index = order_map.find_then_remove(resting_order_id)->get();
    Order& order = order_pool.get(order_index);
    return purge_order(order_index, order);
}

// REQUIRES: order_map to be advanced to order_id via `find_then_remove()`
auto BookState::purge_order(Index order_index, const Order& order) -> BookUpdate {
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
	BookUpdate update = update_bbo_reduce(order); // Update the BBO
	order_pool.free(order_index);
	return update;
}

auto BookState::cancel(OrderID resting_order_id) -> BookUpdate {
   return purge_order(resting_order_id);
}

/*
- Find the order_index via the order map
- Check the price level and reduce quantity - erase the price level if it hits 0
- Erase the order from the order_map and deallocate it from the order pool if its quantity hit 0
*/
auto BookState::trade(OrderID resting_order_id, Quantity quantity) -> BookUpdate {
    Index order_index = order_map.find_then_remove(resting_order_id)->get();
    Order& order = order_pool.get(order_index);

	// Logically same as purge order from book's perspective
    if(order.quantity == quantity) {
        return purge_order(order_index, order);
    }

	// Partial order removal
	PriceLevel& price_level = price_levels.find(order.price)->get();
	Quantity new_order_quantity = order.quantity - quantity;
	price_level.total_quantity -= quantity;

	order.quantity = quantity;              // Mark order with delta for BBO update
	BookUpdate update = update_bbo_reduce(order);
	order.quantity = new_order_quantity;    // Restore true order quantity
	return update;
}

auto BookState::cancel(const Order& order) -> BookUpdate {
    return cancel(order.order_id);
}

auto BookState::trade(const Order& order) -> BookUpdate {
    return trade(order.order_id, order.quantity);
}

auto BookState::execute(const Order& order) -> BookUpdate {
	switch(order.type) {
		case Order::Type::Add:
			return add(order);
		case Order::Type::Cancel:
			return cancel(order);
		case Order::Type::Trade:
			return trade(order);
		default:
			assert(false && "[BookState] Execute: Unreachable");
			std::unreachable();
	}
}