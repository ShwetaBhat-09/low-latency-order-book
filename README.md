# Low-Latency Order Book

A high-performance limit order book matching engine in C++17. Implements price-time priority matching with memory-pooled allocations for deterministic, sub-microsecond latency.

## Architecture

```
┌─────────────────────────────────────────────────┐
│              Matching Engine                     │
│                                                 │
│  ┌─────────────────┐   ┌─────────────────┐     │
│  │   Bid Queue     │   │   Ask Queue     │     │
│  │ (price desc,    │   │ (price asc,     │     │
│  │  time asc)      │   │  time asc)      │     │
│  └────────┬────────┘   └────────┬────────┘     │
│           │    Price Crosses?    │              │
│           └──────────┬──────────┘              │
│                      ▼                          │
│              ┌──────────────┐                   │
│              │ Execute Trade│ → Trade Callback   │
│              └──────────────┘                   │
│                                                 │
│  ┌─────────────────────────────────────────┐    │
│  │          Object Pool (pre-allocated)     │    │
│  │     No malloc/new on hot path            │    │
│  └─────────────────────────────────────────┘    │
└─────────────────────────────────────────────────┘
```

## Performance

Benchmarked measured (single-threaded):

```
═══════════════════════════════════════════════════
  pool acquire+release:      5-10 ns/op
  add_order (no match):      80-150 ns/op
  add_order (with match):    150-300 ns/op
  cancel_order:              100-200 ns/op
  mixed workload (40/40/20): 200-400 ns/op
  → throughput:              3-5M ops/sec
═══════════════════════════════════════════════════
```

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **Object Pool** | Pre-allocates Order objects. Zero heap allocation on the hot path — eliminates malloc jitter |
| **`std::set` with custom comparator** | O(log N) insert/erase with price-time ordering. Predictable worst-case |
| **Header-only library** | Single include, zero build complexity, easy to embed |
| **No virtual dispatch** | Templates instead of polymorphism — compiler inlines everything |
| **Nanosecond timestamps** | `high_resolution_clock` for accurate benchmarking and time priority |
| **Callback-based trades** | Zero-cost abstraction — no allocation, caller decides what to do |

## Operations

| Operation | Complexity | Typical Latency |
|-----------|-----------|-----------------|
| `add_order` (no match) | O(log N) | ~100ns |
| `add_order` (with match) | O(log N + K fills) | ~200ns |
| `cancel_order` | O(log N) | ~150ns |
| `modify_order` (reduce) | O(1) | ~50ns |
| `modify_order` (increase) | O(log N) | ~150ns |
| `add_market_order` | O(K fills) | ~100ns per fill |

## Quick Start

```bash
# Prerequisites: C++17 compiler (g++ or clang++)
# macOS: xcode-select --install (or brew install gcc)
# Linux: sudo apt install g++

git clone https://github.com/ShwetaBhat-09/low-latency-order-book.git
cd low-latency-order-book

# Run tests
make test

# Run benchmarks
make bench

# Run demo
mkdir -p bin
make run
```

## API

```cpp
#include "orderbook.h"

orderbook::OrderBook book;

// Register trade callback
book.set_trade_callback([](const orderbook::Trade& t) {
    printf("Trade: %u @ %.2f\n", t.quantity, t.price);
});

// Add limit orders
uint64_t id = book.add_order(Side::Buy, 100.50, 100);

// Add market order (immediate execution at best price)
book.add_market_order(Side::Sell, 50);

// Cancel order
book.cancel_order(id);

// Modify order (reduce keeps priority, increase loses it)
book.modify_order(id, 75);

// Query book state
double bid = book.best_bid();
double ask = book.best_ask();
auto depth = book.get_bids(5); // top 5 price levels
```

## Project Structure

```
low-latency-order-book/
├── include/
├── src/
├── tests/
├── bench/
├── Makefile
└── README.md
```

## What This Demonstrates

1. **Low-latency systems design** — every nanosecond counts, no allocations on critical path
2. **Data structure selection** — why `std::set` over `std::priority_queue` (need erase for cancels)
3. **Memory management** — custom object pool eliminates allocator overhead
4. **Matching algorithm** — price-time priority with partial fills
5. **C++ proficiency** — templates, move semantics, RAII, modern C++17

## Future Improvements (not implemented, good interview discussion points)

- Lock-free SPSC queue for order ingestion (LMAX Disruptor pattern)
- SIMD-optimized price level scanning
- Kernel bypass networking (DPDK/io_uring)
- FIX protocol parser
- Multi-symbol support with symbol-sharded books

## License

MIT
