#include "engine.hpp"
#include <algorithm>
#include <thread>

std::atomic<intmax_t> global_timestamp_counter(0);

inline intmax_t getCurrentTimestamp() {
    return global_timestamp_counter.fetch_add(1, std::memory_order_relaxed);
}

bool Order::operator<(const Order& other) const {
    if (this->type == CommandType::input_buy) {
        return this->price > other.price ||
            (this->price == other.price && this->timestamp < other.timestamp);
    }

    if (this->type == CommandType::input_sell) {
        return this->price < other.price ||
            (this->price == other.price && this->timestamp < other.timestamp);
    }
    throw std::runtime_error("Invalid order type");
}

// OrderBook functions
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
    Output::OrderAdded(order.order_id, order.instrument, order.price, order.count, false, order.timestamp);
}

void OrderBook::add_sell_order(Order&& order) {
    auto it = std::lower_bound(sellOrders.begin(), sellOrders.end(), order,
        [](const Order& a, const Order& b) { return a.price < b.price || (a.price == b.price && a.timestamp < b.timestamp); });
    sellOrders.insert(it, std::move(order));
    Output::OrderAdded(order.order_id, order.instrument, order.price, order.count, true, order.timestamp);
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

            Output::OrderExecuted(it->order_id, active_order.order_id, 1, it->price,
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
        std::unique_lock lock(mutex);
        auto it = std::find_if(buyOrders.begin(), buyOrders.end(), [order_id](const Order& o) { return o.order_id == order_id; });
        if (it != buyOrders.end()) {
            buyOrders.erase(it);
            Output::OrderDeleted(order_id, true, getCurrentTimestamp());
            return; // Order found and erased
        }
    }
    { // Attempt to cancel from sellOrders
        std::unique_lock lock(mutex);
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

void Engine::accept(ClientConnection connection) {
    auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
    thread.detach();
}

void Engine::connection_thread(ClientConnection connection) {
    while (true) {
        ClientCommand cmd {};
        auto result = connection.readInput(cmd);

        if (result == ReadResult::Error || result == ReadResult::EndOfFile) {
            return;  // Exit the thread on error or EOF
        }

        if (cmd.type == input_cancel) {
            // Assume cancelation request
            // Find the appropriate order book and try to cancel the order
            auto instrument = orderInstrument[cmd.order_id];
            auto& book = books[instrument];
            book.cancelOrder(cmd.order_id);
        } else {
            // Find or create the order book for this instrument
            auto& book = books[cmd.instrument];
            std::unique_lock lock(book.mutex);

            // New order. Create and try to match
            Order newOrder(cmd.order_id, cmd.price, cmd.count, cmd.type, cmd.instrument);
            book.matchOrder(newOrder);

            if (newOrder.count > 0) {  // Not fully matched, add to order book
                book.addOrder(std::move(newOrder));
                orderInstrument[newOrder.order_id] = newOrder.instrument;
            }
        }
    }
}
