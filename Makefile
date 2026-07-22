CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -I include
DEBUG_FLAGS = -std=c++17 -g -Wall -Wextra -I include -fsanitize=address

.PHONY: all clean test bench demo

all: demo test bench

# Demo binary
demo: src/main.cpp include/orderbook.h include/order.h include/memory_pool.h
	$(CXX) $(CXXFLAGS) -o bin/orderbook src/main.cpp

# Test binary
test: tests/test_orderbook.cpp include/orderbook.h
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o bin/test_orderbook tests/test_orderbook.cpp
	@echo "Running tests..."
	@./bin/test_orderbook

# Benchmark binary
bench: bench/benchmark.cpp include/orderbook.h
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -march=native -o bin/benchmark bench/benchmark.cpp
	@echo "Running benchmarks..."
	@./bin/benchmark

# Debug build with AddressSanitizer
debug: tests/test_orderbook.cpp include/orderbook.h
	@mkdir -p bin
	$(CXX) $(DEBUG_FLAGS) -o bin/test_debug tests/test_orderbook.cpp
	./bin/test_debug

# Run demo
run: demo
	./bin/orderbook

clean:
	rm -rf bin/
