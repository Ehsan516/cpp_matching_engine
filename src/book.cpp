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

// one loop for both sides. opp = the other side's map, begin() is always its best price
// template only bc asks and bids are different map types (the comparator), nothing fancy
template<class M, class Cross>
static void sweep(M &opp, Order &in, Cross crosses, std::vector<Trade> &trades)
{
    while(in.qty>0 && !opp.empty()) {
        auto lvl = opp.begin();
        if(!crosses(lvl->first)) break;   // best price is outside our limit, stop

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
        if(q.empty()) opp.erase(lvl);
    }
}

std::vector<Trade> OrderBook::match(Order in)
{
    std::vector<Trade> trades;
    const Px lim = in.px;

    if(in.side==Side::Buy)
        sweep(asks, in, [lim](Px p){ return p <= lim; }, trades);
    else
        sweep(bids, in, [lim](Px p){ return p >= lim; }, trades);
    //  if(lvl->first > in.px) break;    <- old buy version
    //  if(lvl->first < in.px) break;    <- old sell version

    if(in.qty > 0) add_resting(in);
    return trades;
}

// dumb linear scan over one side. O(n) but easy to get right first
template<class M>
static Qty scan_cancel(M &side, OrderId id)
{
    for(auto lv = side.begin(); lv != side.end(); ++lv) {
        auto &q = lv->second;
        for(auto it = q.begin(); it!=q.end(); ++it) {
            if(it->id == id) {
                Qty r = it->qty;
                q.erase(it);
                if(q.empty()) side.erase(lv);
                return r;   // returning straight away so the dead iterators never get used
            }
        }
    }
    return 0;
}

Qty OrderBook::cancel(OrderId id)
{
    Qty r = scan_cancel(bids, id);
    if(r==0) r = scan_cancel(asks, id);
    return r;
}

template<class M>
static const char* check_side(const M &side, Side want, std::unordered_set<OrderId> &ids)
{
    for(auto &kv : side) {
        if(kv.second.empty()) return "empty level";
        std::uint64_t lastSeq=0;
        for(auto& o : kv.second) {
            if(o.qty<=0) return "non positive qty resting";
            if(o.side!=want || o.px!=kv.first) return "order in wrong place";
            if(o.seq<=lastSeq) return "fifo broken";
            lastSeq=o.seq;
            if(!ids.insert(o.id).second) return "dup id in book";
        }
    }
    return nullptr;
}

const char* OrderBook::check() const
{
    std::unordered_set<OrderId> ids;
    const char* why = check_side(bids, Side::Buy, ids);
    if(!why) why = check_side(asks, Side::Sell, ids);
    if(why) return why;
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
