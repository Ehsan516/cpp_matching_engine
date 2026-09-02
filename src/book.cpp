#include "engine/book.h"
#include <limits>
#include <stdexcept>

namespace eng {

static Qty add_chk(Qty a, Qty b) {
    if(b > 0 && a > std::numeric_limits<Qty>::max() - b)
        throw std::overflow_error("level qty overflow");
    return a+b;
}

void OrderBook::add_resting(const Order &o)
{
    if(o.side == Side::Sell) {
        asks[o.px].push_back(o);
    }
    else
        bids[o.px].push_back(o);
}

std::vector<Lvl> OrderBook::snapshot() const
{
    std::vector<Lvl> out;
    for (auto& kv : bids) {
        Qty t=0;
        for(auto &o : kv.second) t = add_chk(t, o.qty);
        out.push_back({Side::Buy, kv.first, t});
    }
    for (auto& kv : asks) {
        Qty t=0;
        for(auto &o : kv.second) t = add_chk(t, o.qty);
        // t += o.qty;
        out.push_back({Side::Sell, kv.first, t});
    }
    return out;
}

}
