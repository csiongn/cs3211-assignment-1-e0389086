#include "string.h"

#include "Clock.hpp"
#include "io.hpp"
#include "Order.hpp"
#include "OrderBook.hpp"

void OrderBook::addOrder(Order&& order) {
	if (order.type == input_buy) {
		add_buy_order(std::move(order));
	} else if (order.type == input_sell) {
		add_sell_order(std::move(order));
	}
}

void OrderBook::add_buy_order(Order&& order) {
    auto it = std::lower_bound(buyOrders.begin(), buyOrders.end(), order,
        [](const Order& a, const Order& b) { return a.price > b.price || (a.price == b.price && a.timestamp < b.timestamp); });
    buyOrders.insert(it, std::move(order));
    Output::OrderAdded(order.order_id, order.instrument, order.price, order.count, false, getCurrentTimestamp());
}

void OrderBook::add_sell_order(Order&& order) {
    auto it = std::lower_bound(sellOrders.begin(), sellOrders.end(), order,
        [](const Order& a, const Order& b) { return a.price < b.price || (a.price == b.price && a.timestamp < b.timestamp); });
    sellOrders.insert(it, std::move(order));
    Output::OrderAdded(order.order_id, order.instrument, order.price, order.count, true, getCurrentTimestamp());
}

std::vector<Order> OrderBook::matchOrder(Order& active_order) {
    std::vector<Order>& resting_orders = (active_order.type == input_sell) ? buyOrders : sellOrders;
    std::vector<Order> matched;

    for (auto it = resting_orders.begin(); it != resting_orders.end() && active_order.count > 0;) {
        if ((active_order.type == input_sell && active_order.price <= it->price) ||
            (active_order.type == input_buy && active_order.price >= it->price)) {
            uint32_t trade_count = std::min(active_order.count, it->count);
            active_order.count -= trade_count;
            it->count -= trade_count;

            Output::OrderExecuted(it->order_id, active_order.order_id, ++it->execution_count, it->price,
                    trade_count, getCurrentTimestamp());

            // If a resting order is fully matched, move to matched vector
            if (it->count == 0) {
                matched.push_back(std::move(*it));
                it = resting_orders.erase(it); // Remove fully matched order
            } else {
                // If partially matched, push a copy with traded volume
                Order partial_matched_order = *it;
                partial_matched_order.count = trade_count;

                matched.push_back(std::move(partial_matched_order));
                ++it;
            }
        } else {
            break; // No more matching orders
        }
    }
    return matched;
}

void OrderBook::cancelOrder(uint32_t order_id) {
    { // Attempt to cancel from buyOrders first
        auto it = std::find_if(buyOrders.begin(), buyOrders.end(), [order_id](const Order& o) { return o.order_id == order_id; });
        if (it != buyOrders.end()) {
            buyOrders.erase(it);
            Output::OrderDeleted(order_id, true, getCurrentTimestamp());
            return; // Order found and erased
        }
    }
    { // Attempt to cancel from sellOrders
        auto it = std::find_if(sellOrders.begin(), sellOrders.end(), [order_id](const Order& o) { return o.order_id == order_id; });
        if (it != sellOrders.end()) {
            sellOrders.erase(it);
            Output::OrderDeleted(order_id, true, getCurrentTimestamp());
            return; // Order found and erased
        }
    }
    // If order is not found in either side, it may be due to it having been executed or never existed.
    Output::OrderDeleted(order_id, false, getCurrentTimestamp());
}