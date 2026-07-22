#include "../include/orderbook.h"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>

using namespace orderbook;

// Benchmark helper
template <typename Func>
double benchmark_ns(Func f, int iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        f();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return static_cast<double>(duration) / iterations;
}

void bench_add_order_no_match() {
    OrderBook book;
    int i = 0;

    double ns = benchmark_ns([&]() {
        // Alternating buy/sell with spread so no matching
        if (i % 2 == 0) {
            book.add_order(Side::Buy, 99.0 - (i % 100) * 0.01, 100);
        } else {
            book.add_order(Side::Sell, 101.0 + (i % 100) * 0.01, 100);
        }
        i++;
    }, 1000000);

    std::cout << "  add_order (no match):     " << std::fixed << std::setprecision(1) << ns << " ns/op\n";
}

void bench_add_order_with_match() {
    OrderBook book;

    // Pre-fill the book with sells
    for (int i = 0; i < 1000; i++) {
        book.add_order(Side::Sell, 100.0 + i * 0.01, 1);
    }

    int i = 0;
    double ns = benchmark_ns([&]() {
        // Each buy matches against best ask
        book.add_order(Side::Buy, 200.0, 1);
        // Refill to keep book populated
        book.add_order(Side::Sell, 100.0 + (i % 1000) * 0.01, 1);
        i++;
    }, 500000);

    std::cout << "  add_order (with match):   " << std::fixed << std::setprecision(1) << ns << " ns/op\n";
}

void bench_cancel_order() {
    OrderBook book;
    std::vector<uint64_t> ids;
    ids.reserve(100000);

    for (int i = 0; i < 100000; i++) {
        ids.push_back(book.add_order(Side::Buy, 99.0 - (i % 100) * 0.01, 100));
    }

    size_t i = 0;
    double ns = benchmark_ns([&]() {
        if (i < ids.size()) {
            book.cancel_order(ids[i]);
            i++;
        }
    }, 100000);

    std::cout << "  cancel_order:             " << std::fixed << std::setprecision(1) << ns << " ns/op\n";
}

void bench_mixed_workload() {
    OrderBook book;
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> price_dist(95.0, 105.0);
    std::uniform_int_distribution<int> qty_dist(1, 100);
    std::uniform_int_distribution<int> action_dist(0, 9);
    std::vector<uint64_t> active_ids;
    active_ids.reserve(10000);

    double ns = benchmark_ns([&]() {
        int action = action_dist(rng);
        double price = price_dist(rng);
        uint32_t qty = qty_dist(rng);

        if (action < 4) {
            // 40% buy orders
            uint64_t id = book.add_order(Side::Buy, price, qty);
            if (active_ids.size() < 10000) active_ids.push_back(id);
        } else if (action < 8) {
            // 40% sell orders
            uint64_t id = book.add_order(Side::Sell, price, qty);
            if (active_ids.size() < 10000) active_ids.push_back(id);
        } else {
            // 20% cancels
            if (!active_ids.empty()) {
                size_t idx = rng() % active_ids.size();
                book.cancel_order(active_ids[idx]);
                active_ids[idx] = active_ids.back();
                active_ids.pop_back();
            }
        }
    }, 1000000);

    std::cout << "  mixed workload (40/40/20):" << std::fixed << std::setprecision(1) << ns << " ns/op\n";
    std::cout << "  → throughput:             " << std::fixed << std::setprecision(1) << (1e9 / ns) / 1e6 << "M ops/sec\n";
    std::cout << "  → trades executed:        " << book.total_trades() << "\n";
}

void bench_memory_pool() {
    ObjectPool<Order> pool(100000);

    double ns = benchmark_ns([&]() {
        Order* o = pool.acquire();
        pool.release(o);
    }, 10000000);

    std::cout << "  pool acquire+release:     " << std::fixed << std::setprecision(1) << ns << " ns/op\n";
}

int main() {
    std::cout << "Order Book Benchmarks:\n";
    std::cout << "===================================================\n";

    bench_memory_pool();
    bench_add_order_no_match();
    bench_add_order_with_match();
    bench_cancel_order();
    bench_mixed_workload();

    std::cout << "===================================================\n";
    std::cout << "Done.\n";
    return 0;
}
