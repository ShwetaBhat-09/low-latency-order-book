#include "orderbook.h"
#include <iostream>
#include <iomanip>

using namespace orderbook;

int main() {
    OrderBook book;

    // Register trade callback
    book.set_trade_callback([](const Trade& t) {
        std::cout << "  TRADE: " << t.quantity << " @ " << std::fixed
                  << std::setprecision(2) << t.price
                  << " (buy=" << t.buy_order_id << " sell=" << t.sell_order_id << ")\n";
    });

    std::cout << "=== Low-Latency Order Book Demo ===\n\n";

    // Build up the book
    std::cout << "Adding resting orders...\n";
    book.add_order(Side::Sell, 102.50, 200);
    book.add_order(Side::Sell, 102.00, 150);
    book.add_order(Side::Sell, 101.50, 100);
    book.add_order(Side::Buy, 100.50, 100);
    book.add_order(Side::Buy, 100.00, 200);
    book.add_order(Side::Buy, 99.50, 150);

    std::cout << "\nBook state:\n";
    std::cout << "  Best bid: " << book.best_bid() << "\n";
    std::cout << "  Best ask: " << book.best_ask() << "\n";
    std::cout << "  Spread:   " << book.spread() << "\n";
    std::cout << "  Orders:   " << book.total_orders() << "\n";

    std::cout << "\n  ASK side:\n";
    for (auto& level : book.get_asks(5)) {
        std::cout << "    " << std::fixed << std::setprecision(2) << level.price
                  << " | qty=" << level.total_quantity
                  << " | orders=" << level.order_count << "\n";
    }
    std::cout << "  -------------------\n";
    std::cout << "  BID side:\n";
    for (auto& level : book.get_bids(5)) {
        std::cout << "    " << std::fixed << std::setprecision(2) << level.price
                  << " | qty=" << level.total_quantity
                  << " | orders=" << level.order_count << "\n";
    }

    // Aggressive buy sweeps through asks
    std::cout << "\n--- Aggressive BUY 200 @ 102.00 ---\n";
    book.add_order(Side::Buy, 102.00, 200);

    std::cout << "\nBook after sweep:\n";
    std::cout << "  Best bid: " << book.best_bid() << "\n";
    std::cout << "  Best ask: " << book.best_ask() << "\n";
    std::cout << "  Trades:   " << book.total_trades() << "\n";

    // Market order
    std::cout << "\n--- Market SELL 50 ---\n";
    book.add_market_order(Side::Sell, 50);

    std::cout << "\nFinal state:\n";
    std::cout << "  Total trades: " << book.total_trades() << "\n";
    std::cout << "  Remaining orders: " << book.total_orders() << "\n";

    return 0;
}
