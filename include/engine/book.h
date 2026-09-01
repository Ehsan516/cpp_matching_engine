#pragma once
#include <map>
#include <list>
#include "engine/order.h"

namespace eng {

class OrderBook {
public:
    void add_resting(const Order &o);

    std::size_t askLevels() const { return asks.size(); }
    Px bestAsk() const { return asks.begin()->first; }  // dont call on empty book

private:
    std::map<Px, std::list<Order>> asks;   // lowest px first
    // std::vector<Order> asks;  sorting every insert, nah
};

}
