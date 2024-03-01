#include <atomic>

#include "Clock.hpp"

std::atomic<intmax_t> global_timestamp_counter{0};

intmax_t getCurrentTimestamp() {
    return global_timestamp_counter.fetch_add(1, std::memory_order_seq_cst);
}
