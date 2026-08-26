#include "../order_book.h"
#include "naive_order_book.h"
#include "../order.h"
#include "../types.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Stats {
    double p50_ns;
    double p90_ns;
    double p99_ns;
    double mean_ns;
    double total_ms;
};

Stats summarize(std::vector<long long>& latencies_ns) {
    std::sort(latencies_ns.begin(), latencies_ns.end());
    const std::size_t n = latencies_ns.size();

    auto percentile = [&](double p) {
        std::size_t idx = static_cast<std::size_t>(p * (n - 1));
        return static_cast<double>(latencies_ns[idx]);
    };

    long long sum = 0;
    for (long long v : latencies_ns) sum += v;

    return Stats{
        percentile(0.50),
        percentile(0.90),
        percentile(0.99),
        static_cast<double>(sum) / static_cast<double>(n),
        static_cast<double>(sum) / 1e6,
    };
}

// Times engine.add_order() once per order in `orders`, in the given order,
// on a freshly constructed Engine. `orders` itself is untouched -- add_order
// takes Order by value -- so the exact same input feeds every engine tried.
template <typename Engine>
std::vector<long long> run_trial(const std::vector<Order>& orders) {
    Engine engine;
    std::vector<long long> latencies_ns;
    latencies_ns.reserve(orders.size());

    // match() prints a line per trade; that I/O would dominate the timing
    // and bury the allocator difference this benchmark exists to show, so
    // cout is redirected to a throwaway buffer for the duration of the run.
    std::ostringstream sink;
    std::streambuf* real_cout = std::cout.rdbuf(sink.rdbuf());

    for (const Order& order : orders) {
        auto start = std::chrono::high_resolution_clock::now();
        engine.add_order(order);
        auto end = std::chrono::high_resolution_clock::now();
        latencies_ns.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    std::cout.rdbuf(real_cout);
    return latencies_ns;
}

void print_row(const std::string& name, const Stats& s) {
    std::cout << std::left << std::setw(24) << name << std::right << std::fixed
               << std::setprecision(0)
               << std::setw(10) << s.p50_ns
               << std::setw(10) << s.p90_ns
               << std::setw(10) << s.p99_ns
               << std::setw(10) << s.mean_ns
               << std::setprecision(2)
               << std::setw(12) << s.total_ms
               << std::endl;
}

// feed_simulator's generateX() keeps bid/ask prices within +-5 of the same
// base price, so almost every order crosses the book immediately and gets
// matched away again a few microseconds later -- the book never grows
// beyond a handful of resting orders. That's realistic for the live demo,
// but it hides exactly the thing this benchmark wants to measure: the cost
// of add_order() as a deep, sustained book of resting orders builds up (the
// scenario MemoryPool + the intrusive list were built for in steps 4/5).
// So here, bids and asks are drawn from disjoint price ranges that can
// never cross, and every one of the N orders ends up resting.
Order make_resting_order(OrderId id, Side side) {
    Price price = (side == Side::Buy)
        ? 1 + static_cast<Price>(std::rand() % 5000)
        : 10000 + static_cast<Price>(std::rand() % 5000);
    Volume volume = 1 + static_cast<Volume>(std::rand() % 100);
    return Order{ id, price, volume, side, static_cast<uint64_t>(id) };
}

} // namespace

int main() {
    constexpr int N = 50000;       // orders per trial
    constexpr int TRIALS = 10;     // + one throwaway warm-up trial per engine

    std::vector<Order> orders;
    orders.reserve(N);
    for (int i = 0; i < N; i++) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        orders.push_back(make_resting_order(i, side));
    }

    // A single 200k-order run swings wildly between invocations on a
    // general-purpose OS with no CPU pinning or real-time priority -- one
    // stray scheduler preemption or frequency-scaling ramp-up is enough to
    // flip which engine looks faster. Running many trials, alternating
    // which engine goes first each time (so neither is systematically
    // favoured by warming caches for the other), and pooling every sample
    // into one distribution is what actually makes p50/p99 trustworthy here.
    run_trial<NaiveOrderBook>(orders); // warm-up, discarded
    run_trial<OrderBook>(orders);      // warm-up, discarded

    std::vector<long long> naive_all;
    std::vector<long long> pool_all;
    naive_all.reserve(static_cast<std::size_t>(N) * TRIALS);
    pool_all.reserve(static_cast<std::size_t>(N) * TRIALS);

    for (int t = 0; t < TRIALS; t++) {
        auto naive_lat = run_trial<NaiveOrderBook>(orders);
        auto pool_lat = run_trial<OrderBook>(orders);
        naive_all.insert(naive_all.end(), naive_lat.begin(), naive_lat.end());
        pool_all.insert(pool_all.end(), pool_lat.begin(), pool_lat.end());
    }

    Stats naive_stats = summarize(naive_all);
    Stats pool_stats = summarize(pool_all);

    std::cout << TRIALS << " trials x " << N
               << " add_order calls, identical order sequence fed to both engines each trial\n\n";
    std::cout << std::left << std::setw(24) << "engine" << std::right
               << std::setw(10) << "p50 ns" << std::setw(10) << "p90 ns"
               << std::setw(10) << "p99 ns" << std::setw(10) << "mean ns"
               << std::setw(12) << "total ms" << std::endl;

    print_row("naive (map+list)", naive_stats);
    print_row("pool+intrusive list", pool_stats);

    std::cout << "\np50 speedup: " << std::fixed << std::setprecision(1)
               << (naive_stats.p50_ns / pool_stats.p50_ns) << "x"
               << "   p99 speedup: " << (naive_stats.p99_ns / pool_stats.p99_ns) << "x"
               << std::endl;
}
