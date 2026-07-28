#include "book-state.hpp"

auto BookState::best_opposing_order(Order::Side side) const noexcept -> Order {
	if((side == Order::Side::Buy && bbo.best_bid.price == NO_BID)) {
		return Order {
			.order_id = Order::INVALID_ORDER_ID,
			.price = NO_BID,
			.quantity = 0,
			.side = Order::Side::Buy,
		};
	}
	if((side == Order::Side::Sell && bbo.best_offer.price == NO_OFFER)) {
		return Order {
			.order_id = Order::INVALID_ORDER_ID,
			.price = NO_OFFER,
			.quantity = 0,
			.side = Order::Side::Sell,
		};
	}

	if(side == Order::Side::Buy) {
		Order::ID head_order_id = price_levels.find(bbo.best_bid.price)->get().head;
		Order::Quantity head_order_quantity = order_pool.get(order_map.find(head_order_id)->get()).order.quantity;

		return Order {
			.order_id = head_order_id,
			.price = bbo.best_bid.price,
			.quantity = head_order_quantity,
			.side = Order::Side::Buy,
		};
	}

	Order::ID head_order_id = price_levels.find(bbo.best_offer.price)->get().head;
	Order::Quantity head_order_quantity = order_pool.get(order_map.find(head_order_id)->get()).order.quantity;

	return Order {
		.order_id = head_order_id,
		.price = bbo.best_offer.price,
		.quantity = head_order_quantity,
		.side = Order::Side::Sell,
	};
}

bool BookState::order_exists(Order::ID order_id) const noexcept {
	return order_map.find(order_id).has_value() ? true : false;
}

auto BookState::update_best_bid_add(const Order& order) -> Update {
    if(order.price > bbo.best_bid.price) {
        bbo.best_bid.price = order.price;
        bbo.best_bid.quantity = order.quantity;
        return Update::NewBestBid;
    }
    else if(order.price == bbo.best_bid.price) {
        bbo.best_bid.quantity += order.quantity;
        return Update::NewBestBid;
    }
	return Update::NoChange;
}

auto BookState::update_best_offer_add(const Order& order) -> Update {
    if(order.price < bbo.best_offer.price) {
        bbo.best_offer.price = order.price;
        bbo.best_offer.quantity = order.quantity;
        return Update::NewBestOffer;
    }
    else if(order.price == bbo.best_offer.price) {
        bbo.best_offer.quantity += order.quantity;
        return Update::NewBestOffer;
    }
	return Update::NoChange;
}

auto BookState::update_bbo_add(const Order& order) -> Update {
    if(order.side == Order::Side::Buy) {
        return update_best_bid_add(order);
    }
    else {
        return update_best_offer_add(order);
    }
}

auto BookState::update_best_bid_reduce(const Order& order) -> Update {
    if(order.price == bbo.best_bid.price && order.quantity == bbo.best_bid.quantity) {
        bbo.best_bid.price = (!bids.empty()) ? *bids.begin() : NO_BID;
        bbo.best_bid.quantity = (!bids.empty()) ? price_levels.find(bbo.best_bid.price)->get().total_quantity : 0;
        return Update::NewBestBid;
    }
    else if(order.price == bbo.best_bid.price) {
        bbo.best_bid.quantity -= order.quantity;
        return Update::NewBestBid;
    }
	return Update::NoChange;
}

auto BookState::update_best_offer_reduce(const Order& order) -> Update {
    if(order.price == bbo.best_offer.price && order.quantity == bbo.best_offer.quantity) {
        bbo.best_offer.price = (!offers.empty()) ? *offers.begin() : NO_OFFER;
        bbo.best_offer.quantity = (!offers.empty()) ? price_levels.find(bbo.best_offer.price)->get().total_quantity : 0;
        return Update::NewBestOffer;
    }
    else if(order.price == bbo.best_offer.price) {
        bbo.best_offer.quantity -= order.quantity;
        return Update::NewBestOffer;
    }
	return Update::NoChange;
}

auto BookState::update_bbo_reduce(const Order& order) -> Update {
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
auto BookState::add(const MarketEvent& event) -> Update {
	RestingOrder resting_order {
		.order = event.order,
		.previous = RestingOrder::NULL_INDEX,
		.next = RestingOrder::NULL_INDEX,
	};

    RestingOrder::Index order_index = order_pool.allocate(resting_order);
    order_map.add(resting_order.order.order_id, order_index);

    auto result = price_levels.find_then_add(resting_order.order.price);
    if(!result) {
        price_levels.add_on_saved_index(resting_order.order.price, PriceLevel {
            .head = order_index,
            .tail = order_index,
            .total_quantity = resting_order.order.quantity,
        });

        add_bid_or_offer(resting_order.order);
    }
    else {
        PriceLevel& price_level = *result;
        price_level.total_quantity += resting_order.order.quantity;
        order_pool.get(price_level.tail).next = order_index;
        order_pool.get(order_index).previous = price_level.tail;
        price_level.tail = order_index;
    }

    return update_bbo_add(resting_order.order);
}

void BookState::detach(const RestingOrder& order, PriceLevel& price_level) {
    if(order.previous != RestingOrder::NULL_INDEX) {
		order_pool.get(order.previous).next = order.next;
	}
    else price_level.head = order.next;			// Was head
    if(order.next != RestingOrder::NULL_INDEX) {
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
auto BookState::purge_order(Order::ID resting_order_id) -> Update {
    RestingOrder::Index order_index = order_map.find_then_remove(resting_order_id)->get();
    RestingOrder& order = order_pool.get(order_index);
    return purge_order(order_index, order);
}

// REQUIRES: order_map to be advanced to order_id via `find_then_remove()`
auto BookState::purge_order(RestingOrder::Index order_index, const RestingOrder& resting_order) -> Update {
    PriceLevel& price_level = price_levels.find_then_remove(resting_order.order.price)->get();
    price_level.total_quantity -= resting_order.order.quantity;
    if(price_level.total_quantity == 0) {
    	price_levels.remove_on_saved_index();
        remove_bid_or_offer(resting_order.order);
    }
    else {
        detach(resting_order, price_level);
    }

	order_map.remove_on_saved_index();
	Update update = update_bbo_reduce(resting_order.order); // Update the BBO
	order_pool.free(order_index);
	return update;
}

auto BookState::cancel(Order::ID resting_order_id) -> Update {
   return purge_order(resting_order_id);
}

/*
- Find the order_index via the order map
- Check the price level and reduce quantity - erase the price level if it hits 0
- Erase the order from the order_map and deallocate it from the order pool if its quantity hit 0
*/
auto BookState::trade(Order::ID resting_order_id, Order::Quantity quantity) -> Update {
    RestingOrder::Index order_index = order_map.find_then_remove(resting_order_id)->get();
    RestingOrder& resting_order = order_pool.get(order_index);

	// Logically same as purge order from book's perspective
    if(resting_order.order.quantity == quantity) {
        return purge_order(order_index, resting_order);
    }

	// Partial order removal
	PriceLevel& price_level = price_levels.find(resting_order.order.price)->get();
	Order::Quantity new_order_quantity = resting_order.order.quantity - quantity;
	price_level.total_quantity -= quantity;

	resting_order.order.quantity = quantity;              // Mark order with delta for BBO update
	Update update = update_bbo_reduce(resting_order.order);
	resting_order.order.quantity = new_order_quantity;    // Restore true order quantity
	return update;
}

auto BookState::cancel(const MarketEvent& event) -> Update {
    return cancel(event.order.order_id);
}

auto BookState::trade(const MarketEvent& event) -> Update {
    return trade(event.order.order_id, event.order.quantity);
}

auto BookState::execute(const MarketEvent& event) -> Update {
	switch(event.type) {
		case MarketEvent::Type::Add:
			return add(event);
		case MarketEvent::Type::Cancel:
			return cancel(event);
		case MarketEvent::Type::Trade:
			return trade(event);
		default:
			assert(false && "[BookState] Execute: Unreachable");
			std::unreachable();
	}
}