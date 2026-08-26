#pragma once
#include "../order.h"
#include "../types.h"
#include "../trade.h"
#include <map>
#include <list>
#include <unordered_map>
#include <functional>
#include <optional>
#include <vector>
#include <cstddef>

// Baseline for the benchmark: the same price-time-priority matching logic as
// OrderBook, but with the naive storage the project started with -- a
// std::list<Order> node per order (one heap allocation each) instead of the
// MemoryPool + intrusive linked list. This class exists only to be timed
// against OrderBook in benchmark.cpp.
class NaiveOrderBook {
public:
    bool add_order(Order order);
    bool cancel_order(OrderId id);
    bool display(int depth = 5);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;

    std::optional<Volume> order_volume(OrderId id) const;

    std::size_t order_count() const;

private:
    void match();

    struct OrderLocation {
        Price price;
        Side side;
        std::list<Order>::iterator it;
    };

    std::map<Price, std::list<Order>, std::greater<Price>> bids;
    std::map<Price, std::list<Order>> asks;
    std::vector<Trade> trade_log;
    std::unordered_map<OrderId, OrderLocation> order_map;
};
