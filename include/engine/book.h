#pragma once
#include <map>
#include <list>
#include <functional>
#include "engine/order.h"

namespace eng {

class OrderBook {
public:
    void add_resting(const Order &o);

    std::size_t askLevels() const { return asks.size(); }
    Px bestAsk() const { return asks.begin()->first; }  // dont call on empty book
    std::size_t bidLevels() const {return bids.size();}
    Px bestBid() const { return bids.begin()->first; }

private:
    std::map<Px, std::list<Order>> asks;   // lowest px first
    std::map<Px, std::list<Order>, std::greater<Px>> bids;
    // ^ greater so begin() is the highest bid, same as asks.begin() is best
    // std::vector<Order> asks;  sorting every insert, nah
};

}
