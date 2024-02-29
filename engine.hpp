#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <atomic>
#include <chrono>
#include <map>
#include <vector>
#include <shared_mutex>
#include "io.hpp"

// Global atomic counter for timestamping
extern std::atomic<intmax_t> global_timestamp_counter;

// Forward declaration
class OrderBook;

struct Order
{
	uint32_t order_id;
	uint32_t price;
	uint32_t count;
	CommandType type; // Buy or Sell
	std::string instrument;
	intmax_t timestamp;

	Order(uint32_t oid, uint32_t p, uint32_t c, CommandType t, const std::string& is)
	: order_id(oid), price(p), count(c), type(t), instrument(is), // Use atomic counter for timestamp
	  timestamp(global_timestamp_counter.fetch_add(1, std::memory_order_relaxed)) {}

	// Comparator for price-time priority
	bool operator<(const Order& other) const;
};

class OrderBook
{
public:
	// Mutexes for concurrent access
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

struct Engine
{
public:
	Engine() {};
	void accept(ClientConnection conn);

private:
	void connection_thread(ClientConnection conn);
	std::map<std::string, OrderBook> books; // One order book per instrument
	std::map<uint32_t, std::string> orderInstrument; // Instrument for each order id
	mutable std::shared_mutex booksMutex;
};

#endif