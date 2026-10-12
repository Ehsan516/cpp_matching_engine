#pragma once
#include <map>
#include <list>
#include <functional>
#include <vector>
#ifdef ENGINE_INDEX
#include <unordered_map>
#endif
#include "engine/order.h"

namespace eng {

struct Lvl {
    Side side;
    Px px;
    Qty total;
};

class OrderBook {
public:
    void add_resting(const Order &o);
    std::vector<Trade> match(Order in);
    Qty cancel(OrderId id);   // returns removed qty, 0 = not there

    std::size_t askLevels() const { return asks.size(); }
    Px bestAsk() const { return asks.begin()->first; }  // dont call on empty book
    std::size_t bidLevels() const {return bids.size();}
    Px bestBid() const { return bids.begin()->first; }

    std::vector<Lvl> snapshot() const;  // bids high->low then asks low->high

    const char* check() const;   // nullptr if fine, else what broke. slow, tests only

    void reserve(std::size_t n) {
#ifdef ENGINE_INDEX
        idx.reserve(n);
#else
        (void)n;   // nothing to size in the scan build
#endif
    }

private:
    std::map<Px, std::list<Order>> asks;   // lowest px first
    std::map<Px, std::list<Order>, std::greater<Px>> bids;
    // ^ greater so begin() is the highest bid, same as asks.begin() is best
    // std::vector<Order> asks;  sorting every insert, nah

#ifdef ENGINE_INDEX
    // active orders only. id -> where it sits. NOT the same job as the engine's seen set
    // the iterator is a non owning handle into a level's list. list iterators survive
    // inserts/erases of OTHER elements, so it only goes bad when that exact order is erased,
    // and every place that erases an order (fill, cancel) drops the entry first
    struct Loc {
        Side side;
        Px px;
        std::list<Order>::iterator it;
    };
    std::unordered_map<OrderId, Loc> idx;
#endif
};

}
