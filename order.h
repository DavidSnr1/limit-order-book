#pragma once;
#include "types.h";

struct order {
    OrderId id;
    Price price;
    Volume volume;
    Side side;
    uint64_t timestamp;
};