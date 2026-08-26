// built together with test_matching.cpp (that file has DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN),
// this file only contributes more TEST_CASEs. Only the minimum needed to
// trust NaiveOrderBook as a correct benchmark baseline -- OrderBook's own
// test suite already covers the matching rules in depth.
#include "doctest.h"

#include "../benchmark/naive_order_book.h"
#include "../order.h"
#include "../types.h"


static Order make_order(OrderId id, Side side, Price price, Volume volume,
                        uint64_t timestamp = 0) {
    return Order{ id, price, volume, side, timestamp };
}


TEST_CASE("naive: no match when bid is below ask") {
    NaiveOrderBook ob;
    ob.add_order(make_order(1, Side::Buy,  100, 10, 1));
    ob.add_order(make_order(2, Side::Sell, 101, 10, 2));

    CHECK(ob.order_count() == 2);
    CHECK(ob.best_bid() == 100);
    CHECK(ob.best_ask() == 101);
}

TEST_CASE("naive: crossing orders match at the resting order's price") {
    NaiveOrderBook ob;
    ob.add_order(make_order(1, Side::Sell, 100, 10, 1));  // resting
    ob.add_order(make_order(2, Side::Buy,  101, 10, 2));  // aggressor

    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.order_volume(1).has_value());
    CHECK_FALSE(ob.order_volume(2).has_value());
}

TEST_CASE("naive: partial fill leaves the remainder resting") {
    NaiveOrderBook ob;
    ob.add_order(make_order(1, Side::Sell, 100, 10, 1));
    ob.add_order(make_order(2, Side::Buy,  100, 4, 2));

    CHECK(ob.order_count() == 1);
    CHECK(ob.order_volume(1) == 6);
}

TEST_CASE("naive: cancel removes an order and frees an empty price level") {
    NaiveOrderBook ob;
    ob.add_order(make_order(1, Side::Buy, 100, 10, 1));

    CHECK(ob.cancel_order(1));
    CHECK(ob.order_count() == 0);
    CHECK_FALSE(ob.best_bid().has_value());
}

TEST_CASE("naive: cancel on an unknown id returns false") {
    NaiveOrderBook ob;
    CHECK_FALSE(ob.cancel_order(999));
}
