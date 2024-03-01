#include "engine.hpp"
#include <thread>

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
            std::unique_lock lock(book.mutex);
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
