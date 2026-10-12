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

struct CxlRes {
    Err err=Err::None;
    Qty removed=0;
};

class MatchingEngine {
public:
    AddRes add(OrderId id, Side s, Px px, Qty q);
    CxlRes cancel(OrderId id);
    std::vector<Lvl> snapshot() const { return book.snapshot(); }
    const char* check() const {return book.check();}

    void reset();
    // optional. pre size the hash tables for ~n orders so they dont rehash while running
    // reset() throws this away, call it again after
    void reserve(std::size_t n) { seen.reserve(n); book.reserve(n); }

private:
    OrderBook book;
    std::unordered_set<OrderId> seen;
    // ^ every accepted id ever, incl filled/cancelled. never shrinks (until reset)
    std::uint64_t nxtSeq = 1;
};

}
