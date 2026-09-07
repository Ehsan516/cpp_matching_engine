#pragma once
#include <unordered_set>
#include <vector>
#include "engine/book.h"

namespace eng {

enum class Err { None, DUPLICATE_ID, INVALID_VALUE, NOT_FOUND, BAD_COMMAND };

struct AddRes {
    Err err = Err::None;
    std::vector<Trade> trades;
    Qty left = 0;     // >0 means it rested
};

class MatchingEngine {
public:
    AddRes add(OrderId id, Side s, Px px, Qty q);
    std::vector<Lvl> snapshot() const { return book.snapshot(); }

    void reset();

private:
    OrderBook book;
    std::unordered_set<OrderId> seen;
    // ^ every accepted id ever, incl filled/cancelled. never shrinks (until reset)
    std::uint64_t nxtSeq = 1;
};

}
