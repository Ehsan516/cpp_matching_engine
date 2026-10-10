#pragma once
// shared between bench + bench_lat so they run on the exact same inputs (same checksums)
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include "engine/engine.h"
#ifdef __linux__
#include <sys/utsname.h>
#endif

namespace bench {

using namespace eng;

#ifndef BENCH_CXX
#define BENCH_CXX "?"
#define BENCH_FLAGS "?"
#define BENCH_BT "?"
#define BENCH_SHA ""
#endif

inline std::string cpu_name() {
    std::ifstream f("/proc/cpuinfo");
    std::string ln;
    while(std::getline(f, ln))
        if(ln.rfind("model name", 0)==0) {
            auto p = ln.find(':');
            if(p!=std::string::npos) return ln.substr(p+2);
        }
    return "unknown";
}

inline void print_env() {
    std::printf("compiler: %s\n", BENCH_CXX);
    std::printf("build:    %s  flags: %s\n", BENCH_BT, BENCH_FLAGS);
    std::printf("commit:   %s\n", std::strlen(BENCH_SHA) ? BENCH_SHA : "unknown");
#ifdef __linux__
    utsname u;
    if(uname(&u)==0) std::printf("os:       %s %s %s\n", u.sysname, u.release, u.machine);
#endif
    std::printf("cpu:      %s\n", cpu_name().c_str());
#ifdef ENGINE_INDEX
    std::printf("cancel:   id index\n");
#else
    std::printf("cancel:   linear scan\n");
#endif

#ifndef NDEBUG
    std::printf("!! asserts on / not a release build, numbers are not worth recording\n");
#endif
#ifdef BENCH_SAN
    std::printf("!! sanitizers on, numbers are meaningless\n");
#endif
}

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
inline Workload gen_rest(size_t n, std::uint64_t seed) {
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
inline Workload gen_match(size_t n, std::uint64_t seed) {
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
inline Workload gen_cancel(size_t n, std::uint64_t seed) {
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
inline Workload gen_mixed(size_t n, std::uint64_t seed) {
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

inline std::vector<Workload> all_workloads(size_t n, std::uint64_t seed) {
    return { gen_rest(n, seed), gen_match(n, seed), gen_cancel(n, seed), gen_mixed(n, seed) };
}

// fnv-1a over the fields so you can tell if the generated input ever changes
inline std::uint64_t cksum(const Workload& w) {
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

inline bool same(const Out& a, const Out& b) {
    return a.ok==b.ok && a.rej==b.rej && a.trades==b.trades && a.tradedQty==b.tradedQty;
}

// one cmd into the engine, counts go into o. both benches use it so the per cmd work matches
inline void apply(MatchingEngine& e, const BCmd& c, Out& o) {
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

inline Out feed(MatchingEngine& e, const std::vector<BCmd>& v) {
    Out o;
    for(auto& c : v) apply(e, c, o);
    return o;
}

}
