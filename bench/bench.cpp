#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include "engine/engine.h"

using namespace eng;

// same lcg as the tests. std distributions arent portable so not using them
struct Rng {
    std::uint64_t s;
    std::uint64_t next() { s = s*6364136223846793005ULL + 1442695040888963407ULL; return s>>33; }
    std::int64_t in(std::int64_t lo, std::int64_t hi) { return lo + (std::int64_t)(next() % (std::uint64_t)(hi-lo+1)); }
};

enum Op : std::uint8_t { ADD, CXL };

struct BCmd {
    Op op;
    Side side;
    OrderId id;
    Px px;
    Qty qty;
};

struct Workload {
    std::string name;
    std::vector<BCmd> pre;    // preload, NOT timed
    std::vector<BCmd> cmds;   // the timed part
};

// lots of non crossing adds spread over ~1000 prices each side
static Workload gen_rest(size_t n, std::uint64_t seed) {
    Rng r{seed};
    Workload w; w.name = "rest";
    for(size_t i=0;i<n;i++) {
        bool buy = r.in(0,1);
        Px px = buy ? r.in(9000, 9999) : r.in(10001, 11000);
        w.cmds.push_back({ADD, buy ? Side::Buy : Side::Sell, (OrderId)(i+1), px, r.in(1, 100)});
    }
    return w;
}

// preload both sides near the touch, then crossing orders. limits go a few ticks
// through so some of them sweep levels, sizes vary so we get partials
static Workload gen_match(size_t n, std::uint64_t seed) {
    Rng r{seed};
    Workload w; w.name = "match";
    OrderId id=1;
    for(size_t i=0;i<n;i++) {
        w.pre.push_back({ADD, Side::Sell, id++, r.in(10001, 10050), r.in(1,100)});
        w.pre.push_back({ADD, Side::Buy,  id++, r.in(9950, 9999),  r.in(1,100)});
    }
    for(size_t i=0;i<n;i++) {
        bool buy = r.in(0,1);
        Px px = buy ? r.in(10001, 10010) : r.in(9990, 9999);
        w.cmds.push_back({ADD, buy?Side::Buy:Side::Sell, id++, px, r.in(1,150)});
    }
    return w;
}

// big resting book then cancel every one of them in a shuffled order
static Workload gen_cancel(size_t n, std::uint64_t seed) {
    Rng r{seed};
    Workload w; w.name="cancel";
    for(size_t i=0;i<n;i++) {
        bool buy = r.in(0,1);
        Px px = buy ? r.in(9000, 9999) : r.in(10001, 11000);
        w.pre.push_back({ADD, buy?Side::Buy:Side::Sell, (OrderId)(i+1), px, r.in(1,100)});
    }
    std::vector<OrderId> ids;
    for(size_t i=0;i<n;i++) ids.push_back(i+1);
    for(size_t i=n-1;i>0;i--) {   // fisher yates
        size_t j = (size_t)r.in(0, (std::int64_t)i);
        std::swap(ids[i], ids[j]);
    }
    for(auto id : ids) w.cmds.push_back({CXL, Side::Buy, id, 0, 0});
    return w;
}

// intended: 50% passive add, 25% crossing add, 25% cancel of some earlier id
// cancels can miss (already filled) - thats why we report what actually happened
static Workload gen_mixed(size_t n, std::uint64_t seed) {
    Rng r{seed};
    Workload w; w.name = "mixed";
    OrderId id=1;
    for(size_t i=0;i<n/10;i++) {
        bool buy=r.in(0,1);
        Px px = buy ? r.in(9900, 9999) : r.in(10001, 10100);
        w.pre.push_back({ADD, buy?Side::Buy:Side::Sell, id++, px, r.in(1,100)});
    }
    for(size_t i=0;i<n;i++) {
        int k = (int)r.in(0, 3);
        bool buy = r.in(0,1);
        if(k<=1) {
            Px px = buy ? r.in(9900, 9999) : r.in(10001, 10100);
            w.cmds.push_back({ADD, buy?Side::Buy:Side::Sell, id++, px, r.in(1,100)});
        } else if(k==2) {
            Px px = buy ? r.in(10001, 10010) : r.in(9990, 9999);
            w.cmds.push_back({ADD, buy?Side::Buy:Side::Sell, id++, px, r.in(1,100)});
        } else {
            w.cmds.push_back({CXL, Side::Buy, (OrderId)r.in(1, (std::int64_t)id-1), 0, 0});
        }
    }
    return w;
}

// fnv-1a over the fields so you can tell if the generated input ever changes
static std::uint64_t cksum(const Workload& w) {
    std::uint64_t h = 1469598103934665603ULL;
    auto mix=[&](std::uint64_t v) { for(int i=0;i<8;i++) { h ^= (v>>(i*8)) & 0xff; h *= 1099511628211ULL; } };
    for(auto* v : {&w.pre, &w.cmds})
        for(auto &c : *v) { mix(c.op); mix((std::uint64_t)c.side); mix(c.id); mix((std::uint64_t)c.px); mix((std::uint64_t)c.qty); }
    return h;
}

struct Out {
    size_t ok=0, rej=0, trades=0;
    Qty tradedQty=0;
};

static Out feed(MatchingEngine& e, const std::vector<BCmd>& v) {
    Out o;
    for(auto& c : v) {
        if(c.op==ADD) {
            AddRes r = e.add(c.id, c.side, c.px, c.qty);
            if(r.err==Err::None) o.ok++; else o.rej++;
            o.trades += r.trades.size();
            for(auto &t : r.trades) o.tradedQty += t.qty;
        } else {
            CxlRes r = e.cancel(c.id);
            if(r.err==Err::None) o.ok++; else o.rej++;
        }
    }
    return o;
}

int main(int argc, char** argv)
{
    size_t n = 10000;
    std::uint64_t seed = 1;
    for(int i=1;i<argc;i++) {
        if(!std::strcmp(argv[i], "--n") && i+1<argc) n = std::strtoull(argv[++i], nullptr, 10);
        else if(!std::strcmp(argv[i], "--seed") && i+1<argc) seed = std::strtoull(argv[++i], nullptr, 10);
        else { std::fprintf(stderr, "usage: bench [--n N] [--seed S]\n"); return 2; }
    }

    std::vector<Workload> ws = { gen_rest(n, seed), gen_match(n, seed), gen_cancel(n, seed), gen_mixed(n, seed) };

    std::printf("n=%zu seed=%llu\n", n, (unsigned long long)seed);
    for(auto &w : ws) {
        MatchingEngine e;
        feed(e, w.pre);
        Out o = feed(e, w.cmds);
        std::printf("%-7s pre=%-7zu cmds=%-7zu cksum=%016llx  ok=%zu rej=%zu trades=%zu\n",
            w.name.c_str(), w.pre.size(), w.cmds.size(), (unsigned long long)cksum(w), o.ok, o.rej, o.trades);
    }
    return 0;
}
