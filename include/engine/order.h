#pragma once
#include "engine/types.h"

namespace eng {

struct Order {
    OrderId id;
    Side side;
    Px px;
    Qty qty;      // remaining, not original
    std::uint64_t seq;  // arrival order, NOT wall clock
    //Qty orig_qty;
};

struct Trade {
    OrderId maker, taker;
    Px px;  // always the resting (maker) price
    Qty qty;
};

}
