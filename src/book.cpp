#include "engine/book.h"
#include <limits>
#include <stdexcept>
#include <algorithm>
#include <unordered_set>

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

// dumb linear scan over everything. O(n) but easy to get right first
Qty OrderBook::cancel(OrderId id)
{
    for(auto lv = bids.begin(); lv != bids.end(); ++lv) {
        auto &q = lv->second;
        for(auto it = q.begin(); it!=q.end(); ++it) {
            if(it->id == id) {
                Qty r = it->qty;
                q.erase(it);
                if(q.empty()) bids.erase(lv);
                return r;   // returning straight away so the dead iterators never get used
            }
        }
    }
    for(auto lv = asks.begin(); lv != asks.end(); ++lv) {
        auto &q = lv->second;
        for(auto it = q.begin(); it!=q.end(); ++it) {
            if(it->id == id) {
                Qty r = it->qty;
                q.erase(it);
                if(q.empty()) asks.erase(lv);
                return r;
            }
        }
    }
    return 0;
}

const char* OrderBook::check() const
{
    std::unordered_set<OrderId> ids;

    for(auto &kv : bids) {
        if(kv.second.empty()) return "empty bid level";
        std::uint64_t lastSeq=0;
        for(auto& o : kv.second) {
            if(o.qty<=0) return "non positive qty resting";
            if(o.side!=Side::Buy || o.px!=kv.first) return "bid in wrong place";
            if(o.seq<=lastSeq) return "fifo broken";
            lastSeq=o.seq;
            if(!ids.insert(o.id).second) return "dup id in book";
        }
    }
    for(auto &kv : asks) {
        if(kv.second.empty()) return "empty ask level";
        std::uint64_t lastSeq=0;
        for(auto& o : kv.second) {
            if(o.qty<=0) return "non positive qty resting";
            if(o.side!=Side::Sell || o.px!=kv.first) return "ask in wrong place";
            if(o.seq<=lastSeq) return "fifo broken";
            lastSeq=o.seq;
            if(!ids.insert(o.id).second) return "dup id in book";
        }
    }
    // price order is the map's job, no point re-checking that

    if(!bids.empty() && !asks.empty() && bestBid() >= bestAsk())
        return "crossed book";
    return nullptr;
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
