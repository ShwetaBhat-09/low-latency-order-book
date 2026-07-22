#pragma once

#include "order.h"
#include "memory_pool.h"

#include <set>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>
#include <atomic>

namespace orderbook {

// OrderBook implements a limit order book with price-time priority matching.
// Optimized for low-latency: memory pooled orders, no allocations on hot path.
class OrderBook {
public:
    using TradeCallback = std::function<void(const Trade&)>;

    explicit OrderBook(size_t pool_size = 65536)
        : order_pool_(pool_size), next_id_(1) {}

    // Add a limit order. Returns order ID. Matches immediately if price crosses.
    uint64_t add_order(Side side, double price, uint32_t quantity) {
        Order* order = order_pool_.acquire();
        order->id = next_id_++;
        order->side = side;
        order->type = OrderType::Limit;
        order->price = price;
        order->quantity = quantity;
        order->filled = 0;
        order->timestamp = now_ns();

        // Try to match immediately
        match(order);

        // If not fully filled, add to book
        if (!order->is_filled()) {
            if (side == Side::Buy) {
                bids_.insert(order);
            } else {
                asks_.insert(order);
            }
            order_map_[order->id] = order;
        } else {
            order_pool_.release(order);
        }

        return order->id;
    }

    // Add a market order. Matches at best available price.
    uint64_t add_market_order(Side side, uint32_t quantity) {
        Order* order = order_pool_.acquire();
        order->id = next_id_++;
        order->side = side;
        order->type = OrderType::Market;
        order->price = (side == Side::Buy) ? 1e18 : 0.0; // aggressive price
        order->quantity = quantity;
        order->filled = 0;
        order->timestamp = now_ns();

        match(order);

        // Market orders don't rest in the book
        order_pool_.release(order);
        return order->id;
    }

    // Cancel an existing order. Returns true if found and cancelled.
    bool cancel_order(uint64_t order_id) {
        auto it = order_map_.find(order_id);
        if (it == order_map_.end()) return false;

        Order* order = it->second;
        if (order->side == Side::Buy) {
            bids_.erase(order);
        } else {
            asks_.erase(order);
        }

        order_map_.erase(it);
        order_pool_.release(order);
        return true;
    }

    // Modify an existing order's quantity. Maintains time priority if qty reduced.
    bool modify_order(uint64_t order_id, uint32_t new_quantity) {
        auto it = order_map_.find(order_id);
        if (it == order_map_.end()) return false;

        Order* order = it->second;

        if (new_quantity <= order->filled) {
            // Effectively a cancel
            return cancel_order(order_id);
        }

        if (new_quantity < order->quantity) {
            // Reduce quantity — keeps time priority
            order->quantity = new_quantity;
        } else {
            // Increase quantity — loses time priority (re-insert)
            if (order->side == Side::Buy) {
                bids_.erase(order);
                order->quantity = new_quantity;
                order->timestamp = now_ns();
                bids_.insert(order);
            } else {
                asks_.erase(order);
                order->quantity = new_quantity;
                order->timestamp = now_ns();
                asks_.insert(order);
            }
        }
        return true;
    }

    // Set callback for trade events
    void set_trade_callback(TradeCallback cb) { trade_callback_ = std::move(cb); }

    // Getters
    double best_bid() const { return bids_.empty() ? 0.0 : (*bids_.begin())->price; }
    double best_ask() const { return asks_.empty() ? 0.0 : (*asks_.begin())->price; }
    double spread() const { return best_ask() - best_bid(); }
    size_t bid_count() const { return bids_.size(); }
    size_t ask_count() const { return asks_.size(); }
    size_t total_orders() const { return order_map_.size(); }
    uint64_t total_trades() const { return trade_count_; }

    // Get order book depth (top N levels)
    struct PriceLevel {
        double price;
        uint32_t total_quantity;
        int order_count;
    };

    std::vector<PriceLevel> get_bids(int depth = 5) const {
        return get_levels(bids_, depth);
    }

    std::vector<PriceLevel> get_asks(int depth = 5) const {
        return get_levels(asks_, depth);
    }

private:
    void match(Order* incoming) {
        if (incoming->side == Side::Buy) {
            match_against(incoming, asks_);
        } else {
            match_against(incoming, bids_);
        }
    }

    template <typename BookSide>
    void match_against(Order* incoming, BookSide& resting_side) {
        auto it = resting_side.begin();
        while (it != resting_side.end() && !incoming->is_filled()) {
            Order* resting = *it;

            // Check price compatibility
            bool price_matches = (incoming->side == Side::Buy)
                ? incoming->price >= resting->price
                : incoming->price <= resting->price;

            if (!price_matches) break; // No more matches possible (sorted)

            // Execute trade
            uint32_t trade_qty = std::min(incoming->remaining(), resting->remaining());
            double trade_price = resting->price; // price-time priority: resting order's price

            incoming->filled += trade_qty;
            resting->filled += trade_qty;

            // Emit trade
            Trade trade{};
            if (incoming->side == Side::Buy) {
                trade.buy_order_id = incoming->id;
                trade.sell_order_id = resting->id;
            } else {
                trade.buy_order_id = resting->id;
                trade.sell_order_id = incoming->id;
            }
            trade.price = trade_price;
            trade.quantity = trade_qty;
            trade.timestamp = now_ns();
            trade_count_++;

            if (trade_callback_) {
                trade_callback_(trade);
            }

            // Remove filled resting order
            if (resting->is_filled()) {
                it = resting_side.erase(it);
                order_map_.erase(resting->id);
                order_pool_.release(resting);
            } else {
                ++it;
            }
        }
    }

    template <typename BookSide>
    std::vector<PriceLevel> get_levels(const BookSide& side, int depth) const {
        std::vector<PriceLevel> levels;
        double current_price = -1;

        for (auto* order : side) {
            if (order->price != current_price) {
                if ((int)levels.size() >= depth) break;
                levels.push_back({order->price, order->remaining(), 1});
                current_price = order->price;
            } else {
                levels.back().total_quantity += order->remaining();
                levels.back().order_count++;
            }
        }
        return levels;
    }

    static uint64_t now_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }

    // Price-time priority ordered sets
    std::set<Order*, BidComparator> bids_; // highest price first
    std::set<Order*, AskComparator> asks_; // lowest price first

    // Fast lookup by order ID
    std::unordered_map<uint64_t, Order*> order_map_;

    // Memory pool for orders (no malloc on hot path)
    ObjectPool<Order> order_pool_;

    uint64_t next_id_;
    uint64_t trade_count_ = 0;
    TradeCallback trade_callback_;
};

} // namespace orderbook
