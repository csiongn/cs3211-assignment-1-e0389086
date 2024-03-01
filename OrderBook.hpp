#pragma once

#include <algorithm>
#include <queue>
#include <vector>

#include "Order.hpp"

class OrderBook
{
public:
    // Mutex for concurrent access
    mutable std::mutex mutex;

    void addOrder(Order&& order);
    std::vector<Order> matchOrder(Order& order);
    void cancelOrder(uint32_t order_id);

private:
    std::vector<Order> buyOrders;
    std::vector<Order> sellOrders;

    void add_buy_order(Order&& order);
    void add_sell_order(Order&& order);
};
