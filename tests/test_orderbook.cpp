#include "../include/orderbook.h"
#include <cassert>
#include <iostream>
#include <cmath>

using namespace orderbook;

void test_add_and_match() {
    OrderBook book;
    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t) { trades.push_back(t); });

    // Add buy at 100, sell at 99 → should match at 99 (resting order's price)
    book.add_order(Side::Buy, 100.0, 50);
    book.add_order(Side::Sell, 99.0, 30);

    assert(trades.size() == 1);
    assert(trades[0].price == 100.0); // resting was the buy at 100
    assert(trades[0].quantity == 30);
    assert(book.bid_count() == 1); // 20 remaining on buy side
    assert(book.ask_count() == 0);

    std::cout << "  [PASS] test_add_and_match\n";
}

void test_no_match_when_spread_exists() {
    OrderBook book;
    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t) { trades.push_back(t); });

    book.add_order(Side::Buy, 99.0, 100);
    book.add_order(Side::Sell, 101.0, 100);

    assert(trades.empty());
    assert(book.bid_count() == 1);
    assert(book.ask_count() == 1);
    assert(book.best_bid() == 99.0);
    assert(book.best_ask() == 101.0);
    assert(book.spread() == 2.0);

    std::cout << "  [PASS] test_no_match_when_spread_exists\n";
}

void test_partial_fill() {
    OrderBook book;
    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t) { trades.push_back(t); });

    book.add_order(Side::Sell, 100.0, 100);
    book.add_order(Side::Buy, 100.0, 40);

    assert(trades.size() == 1);
    assert(trades[0].quantity == 40);
    assert(book.ask_count() == 1); // 60 remaining
    assert(book.bid_count() == 0);

    std::cout << "  [PASS] test_partial_fill\n";
}

void test_multiple_fills() {
    OrderBook book;
    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t) { trades.push_back(t); });

    // Three sells at different prices
    book.add_order(Side::Sell, 100.0, 30);
    book.add_order(Side::Sell, 101.0, 30);
    book.add_order(Side::Sell, 102.0, 30);

    // One big buy sweeps through
    book.add_order(Side::Buy, 102.0, 80);

    assert(trades.size() == 3);
    assert(trades[0].price == 100.0); // best ask first
    assert(trades[0].quantity == 30);
    assert(trades[1].price == 101.0);
    assert(trades[1].quantity == 30);
    assert(trades[2].price == 102.0);
    assert(trades[2].quantity == 20); // only 20 remaining

    assert(book.ask_count() == 1); // 10 left at 102
    assert(book.bid_count() == 0);

    std::cout << "  [PASS] test_multiple_fills\n";
}

void test_price_time_priority() {
    OrderBook book;
    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t) { trades.push_back(t); });

    // Two buys at same price — first one should match first (FIFO)
    uint64_t id1 = book.add_order(Side::Buy, 100.0, 50);
    uint64_t id2 = book.add_order(Side::Buy, 100.0, 50);

    book.add_order(Side::Sell, 100.0, 30);

    assert(trades.size() == 1);
    assert(trades[0].buy_order_id == id1); // first buyer gets priority
    assert(trades[0].quantity == 30);
    (void)id2;

    std::cout << "  [PASS] test_price_time_priority\n";
}

void test_cancel_order() {
    OrderBook book;

    uint64_t id = book.add_order(Side::Buy, 100.0, 50);
    assert(book.bid_count() == 1);

    bool cancelled = book.cancel_order(id);
    assert(cancelled);
    assert(book.bid_count() == 0);

    // Cancel non-existent
    assert(!book.cancel_order(99999));

    std::cout << "  [PASS] test_cancel_order\n";
}

void test_modify_order_reduce() {
    OrderBook book;

    uint64_t id = book.add_order(Side::Buy, 100.0, 100);
    book.modify_order(id, 50);

    // Should still be in book with reduced qty
    assert(book.bid_count() == 1);
    auto levels = book.get_bids(1);
    assert(levels[0].total_quantity == 50);

    std::cout << "  [PASS] test_modify_order_reduce\n";
}

void test_market_order() {
    OrderBook book;
    std::vector<Trade> trades;
    book.set_trade_callback([&](const Trade& t) { trades.push_back(t); });

    book.add_order(Side::Sell, 100.0, 50);
    book.add_order(Side::Sell, 101.0, 50);

    // Market buy — takes best available
    book.add_market_order(Side::Buy, 70);

    assert(trades.size() == 2);
    assert(trades[0].price == 100.0);
    assert(trades[0].quantity == 50);
    assert(trades[1].price == 101.0);
    assert(trades[1].quantity == 20);

    assert(book.ask_count() == 1); // 30 left at 101

    std::cout << "  [PASS] test_market_order\n";
}

void test_get_depth() {
    OrderBook book;

    book.add_order(Side::Buy, 100.0, 50);
    book.add_order(Side::Buy, 100.0, 30);
    book.add_order(Side::Buy, 99.0, 40);
    book.add_order(Side::Buy, 98.0, 60);

    auto bids = book.get_bids(3);
    assert(bids.size() == 3);
    assert(bids[0].price == 100.0);
    assert(bids[0].total_quantity == 80); // 50 + 30
    assert(bids[0].order_count == 2);
    assert(bids[1].price == 99.0);
    assert(bids[2].price == 98.0);

    std::cout << "  [PASS] test_get_depth\n";
}

void test_large_volume() {
    OrderBook book;
    int trade_count = 0;
    book.set_trade_callback([&](const Trade&) { trade_count++; });

    // Add 10000 orders on each side
    for (int i = 0; i < 10000; i++) {
        book.add_order(Side::Buy, 90.0 + (i % 10), 100);
        book.add_order(Side::Sell, 100.0 + (i % 10), 100);
    }

    // All buys at 99 or below, all sells at 100 or above → no matches
    assert(trade_count == 0);
    assert(book.bid_count() == 10000);
    assert(book.ask_count() == 10000);

    std::cout << "  [PASS] test_large_volume\n";
}

int main() {
    std::cout << "Running Order Book Tests:\n";

    test_add_and_match();
    test_no_match_when_spread_exists();
    test_partial_fill();
    test_multiple_fills();
    test_price_time_priority();
    test_cancel_order();
    test_modify_order_reduce();
    test_market_order();
    test_get_depth();
    test_large_volume();

    std::cout << "\nAll tests passed! ✓\n";
    return 0;
}
