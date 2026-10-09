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
    std::list<Order> *q;
    if(o.side == Side::Sell) {
        q = &asks[o.px];
    }
    else
        q = &bids[o.px];
    q->push_back(o);
#ifdef ENGINE_INDEX
    idx[o.id] = Loc{o.side, o.px, std::prev(q->end())};
#endif
}

// one loop for both sides. opp = the other side's map, begin() is always its best price
// template only bc asks and bids are different map types (the comparator), nothing fancy
// gone(id) gets called right before a filled maker is erased
template<class M, class Cross, class Gone>
static void sweep(M &opp, Order &in, Cross crosses, Gone gone, std::vector<Trade> &trades)
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
            if(rest.qty==0) {
                gone(rest.id);
                q.pop_front();
            }
            // ^ rest is dangling after this, dont touch it
        }
        if(q.empty()) opp.erase(lvl);
    }
}

std::vector<Trade> OrderBook::match(Order in)
{
    std::vector<Trade> trades;
    const Px lim = in.px;
#ifdef ENGINE_INDEX
    auto gone = [this](OrderId id){ idx.erase(id); };
#else
    auto gone = [](OrderId){};
#endif

    if(in.side==Side::Buy)
        sweep(asks, in, [lim](Px p){ return p <= lim; }, gone, trades);
    else
        sweep(bids, in, [lim](Px p){ return p >= lim; }, gone, trades);
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

#ifdef ENGINE_INDEX
// index version. hash lookup (avg O(1), not guaranteed) + map find for the level (O(log levels))
// + list erase at a known spot (O(1)). so O(log L) overall, not O(1)
template<class M>
static Qty erase_at(M &side, Px px, std::list<Order>::iterator it)
{
    auto lv = side.find(px);   // has to exist, the index says so
    Qty r = it->qty;
    lv->second.erase(it);
    if(lv->second.empty()) side.erase(lv);
    return r;
}

Qty OrderBook::cancel(OrderId id)
{
    auto f = idx.find(id);
    if(f == idx.end()) return 0;
    Loc l = f->second;
    idx.erase(f);    // drop the entry first, then the order it points at
    return l.side==Side::Buy ? erase_at(bids, l.px, l.it) : erase_at(asks, l.px, l.it);
}
#else
Qty OrderBook::cancel(OrderId id)
{
    Qty r = scan_cancel(bids, id);
    if(r==0) r = scan_cancel(asks, id);
    return r;
}
#endif

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

#ifdef ENGINE_INDEX
    // every resting order has an entry (sizes match + ids are unique)
    // and every entry points at a live order with the same id/side/px
    if(idx.size() != ids.size()) return "index size != resting orders";
    for(auto &kv : idx) {
        if(!ids.count(kv.first)) return "index entry for an order thats not resting";
        const Order &o = *kv.second.it;
        if(o.id!=kv.first || o.side!=kv.second.side || o.px!=kv.second.px) return "stale index entry";
    }
    // ^ if an entry somehow pointed at an erased node the deref itself is UB.
    //   the size/id checks catch most of that first, asan build catches the rest
#endif

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
