CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
CXXFLAGS_BENCH = -std=c++17 -O2

all: app test

app: main.cpp feed_simulator.cpp order_book.cpp memory_pool.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test: tests/test_matching.cpp tests/test_memory_pool.cpp tests/test_spsc_queue.cpp tests/test_naive_order_book.cpp order_book.cpp memory_pool.cpp benchmark/naive_order_book.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

# built with -O2: an unoptimized build makes both engines slow enough that
# the pool-vs-naive difference this is meant to show gets lost in the noise
bench: benchmark/benchmark.cpp order_book.cpp memory_pool.cpp benchmark/naive_order_book.cpp
	$(CXX) $(CXXFLAGS_BENCH) $^ -o $@

clean:
	rm -f app test bench

.PHONY: all clean bench