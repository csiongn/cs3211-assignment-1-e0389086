#pragma once

#include <atomic>
#include <cstdint>

extern std::atomic<intmax_t> global_timestamp_counter;

intmax_t getCurrentTimestamp();

