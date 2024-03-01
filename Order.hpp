#pragma once
#include <cstdint>
#include <utility>

#include "io.hpp"
#include "Clock.hpp"

struct Order
{
    uint32_t order_id;
    uint32_t price;
    uint32_t count;
    uint32_t execution_count;
    CommandType type; // Buy or Sell
    char* instrument;
    intmax_t timestamp;

    Order(uint32_t oid, uint32_t p, uint32_t c, CommandType t, char*  is)
    : order_id(oid), price(p), count(c), execution_count(0), type(t), instrument(std::move(is)), // Use atomic counter for timestamp
      timestamp(global_timestamp_counter.fetch_add(1, std::memory_order_seq_cst)) {}

    // Comparator for price-time priority
    bool operator<(const Order& other) const {
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
};
