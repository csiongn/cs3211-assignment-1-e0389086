#pragma once

#include <atomic>
#include <map>
#include <shared_mutex>
#include "io.hpp"
#include "OrderBook.hpp"

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

