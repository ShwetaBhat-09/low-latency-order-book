#pragma once

#include <cstdint>
#include <chrono>

namespace orderbook {

enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1
};

struct Order {
    uint64_t id;
    Side side;
    OrderType type;
    double price;
    uint32_t quantity;
    uint32_t filled;
    uint64_t timestamp; // nanoseconds since epoch

    uint32_t remaining() const { return quantity - filled; }
    bool is_filled() const { return filled >= quantity; }
};

struct Trade {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    double price;
    uint32_t quantity;
    uint64_t timestamp;
};

// Comparators for price-time priority
struct BidComparator {
    // Higher price first, then earlier timestamp (FIFO)
    bool operator()(const Order* a, const Order* b) const {
        if (a->price != b->price) return a->price > b->price;
        return a->timestamp < b->timestamp;
    }
};

struct AskComparator {
    // Lower price first, then earlier timestamp (FIFO)
    bool operator()(const Order* a, const Order* b) const {
        if (a->price != b->price) return a->price < b->price;
        return a->timestamp < b->timestamp;
    }
};

} // namespace orderbook
