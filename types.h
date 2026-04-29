#pragma once; //makes sure the header is only included once during compilation
#include <cstdint>; //imports fixed-width integer types from the C++ standard library

using OrderId = uint64_t; //defines a new type alias OrderId for uint64_t, which is an unsigned 64-bit integer
using Price = uint64_t; //Price as an unsingned 64-bit int, as it represents the price in cents to avoid floating-point precision issues
using Volume = uint64_t; //Volume as an unsigned 64-bit int, as it represents the number of shares or contracts, which cannot be negative

enum class Side {
    Buy,
    Sell
};

enum class MessageType {
    Add,
    Cancel,
    Execute
};

