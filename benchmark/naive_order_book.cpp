#include "naive_order_book.h"
#include <algorithm>
#include <iostream>

bool NaiveOrderBook::add_order(Order order){
    if (order.side == Side::Buy){
        auto& list = bids[order.price];
        list.push_back(order);
        order_map[order.id] = {order.price, Side::Buy, std::prev(list.end())};
    }
    else if (order.side == Side::Sell){
        auto& list = asks[order.price];
        list.push_back(order);
        order_map[order.id] = {order.price, Side::Sell, std::prev(list.end())};
    }
    match();
    return true;
}

bool NaiveOrderBook::cancel_order(OrderId o_id){
    auto it = order_map.find(o_id);
    if (it == order_map.end()) return false;

    OrderLocation& location = it->second;

    if (location.side == Side::Buy) {
        auto priceIt = bids.find(location.price);
        priceIt->second.erase(location.it);
        if (priceIt->second.empty()) bids.erase(priceIt);
    } else {
        auto priceIt = asks.find(location.price);
        priceIt->second.erase(location.it);
        if (priceIt->second.empty()) asks.erase(priceIt);
    }

    order_map.erase(it);
    return true;
}

void NaiveOrderBook::match() {
    while (!bids.empty() && !asks.empty()) {
        auto bid_it = bids.begin();
        auto ask_it = asks.begin();
        if (bid_it->first < ask_it->first) break;

        auto& bid_list = bid_it->second;
        auto& ask_list = ask_it->second;
        Order& b = bid_list.front();
        Order& a = ask_list.front();

        Volume trade_vol = std::min(b.volume, a.volume);
        b.volume -= trade_vol;
        a.volume -= trade_vol;

        OrderId a_id = a.id;
        OrderId b_id = b.id;

        const Order& resting_o = (a.timestamp < b.timestamp) ? a : b;

        Trade trade = {
            resting_o.price,
            trade_vol,
            b_id,
            a_id
        };

        trade_log.push_back(trade);

        std::cout << "Trade @ Quantity " << trade.qty << " @ Price: " << trade.price << "\n";

        if (b.volume == 0) {
            order_map.erase(b_id);
            bid_list.pop_front();
            if (bid_list.empty()) bids.erase(bid_it);
        }
        if (a.volume == 0) {
            order_map.erase(a_id);
            ask_list.pop_front();
            if (ask_list.empty()) asks.erase(ask_it);
        }
    }
}

bool NaiveOrderBook::display (int depth){
    std::cout << "--- BID ---" << std::endl;
    int d = 0;
    for (const auto& [price, list] : bids){
        if (d >= depth) break;
        std::cout << price << " | ";
        for (const Order& order : list){
            std::cout << " [ " << order.volume << " ] ";
        }
        std::cout << std::endl;
        ++d;
    }

    std::cout << "--- ASK ---" << std::endl;
    d = 0;

    for (const auto& [price, list] : asks){
        if (d >= depth) break;
        std::cout << price << " | ";
        for (const Order& order : list){
            std::cout << " [ " << order.volume << " ] ";
        }
        std::cout << std::endl;
        ++d;
    }
    std::cout << std::endl;
    return true;
}

std::optional<Price> NaiveOrderBook::best_bid() const {
    if (bids.empty()) return std::nullopt;
    return bids.begin()->first;
}

std::optional<Price> NaiveOrderBook::best_ask() const {
    if (asks.empty()) return std::nullopt;
    return asks.begin()->first;
}

std::optional<Volume> NaiveOrderBook::order_volume(OrderId id) const {
    auto it = order_map.find(id);
    if (it == order_map.end()) return std::nullopt;
    return it->second.it->volume;
}

std::size_t NaiveOrderBook::order_count() const {
    return order_map.size();
}
