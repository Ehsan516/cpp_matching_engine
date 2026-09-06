#include "engine/book.h"
#include <limits>
#include <stdexcept>
#include <algorithm>

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

std::vector<Trade> OrderBook::match(Order in)
{
    std::vector<Trade> trades;

    if(in.side==Side::Buy) {
        while(in.qty>0 && !asks.empty()) {
            auto lvl = asks.begin();
            if(lvl->first > in.px) break;   // best ask above our limit, stop

            auto &q = lvl->second;
            while(in.qty > 0 && !q.empty()) {
                Order &rest = q.front();
                Qty n = std::min(in.qty, rest.qty);
                trades.push_back({rest.id, in.id, rest.px, n});
                in.qty -= n;
                rest.qty -= n;
                if(rest.qty==0) q.pop_front();
                // ^ rest is dangling after this, dont touch it
            }
            if(q.empty()) asks.erase(lvl);
        }
    }
    else {
        // same thing mirrored. bids.begin() is the highest bid
        while (in.qty>0 && !bids.empty()) {
            auto lvl = bids.begin();
            if(lvl->first < in.px) break;

            auto& q = lvl->second;
            while(in.qty>0 && !q.empty()) {
                Order &rest = q.front();
                Qty n = std::min(in.qty, rest.qty);
                trades.push_back({rest.id, in.id, rest.px, n});
                in.qty-=n;  rest.qty-=n;
                if(rest.qty==0) q.pop_front();
            }
            if(q.empty()) bids.erase(lvl);
        }
    }

    if(in.qty > 0) add_resting(in);
    return trades;
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
