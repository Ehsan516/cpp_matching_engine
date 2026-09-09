#pragma once
#include <map>
#include <list>
#include <functional>
#include <vector>
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

private:
    std::map<Px, std::list<Order>> asks;   // lowest px first
    std::map<Px, std::list<Order>, std::greater<Px>> bids;
    // ^ greater so begin() is the highest bid, same as asks.begin() is best
    // std::vector<Order> asks;  sorting every insert, nah
};

}
