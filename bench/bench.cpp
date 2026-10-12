#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include "common.h"

using namespace bench;

static bool g_rsv = false;   // --reserve

// one trial. reset + preload happen BEFORE the clock starts
// timed = engine add/cancel incl matching + building the result vectors. no parsing, no printing
static double trial(MatchingEngine& e, const Workload& w, Out& o) {
    e.reset();
    if(g_rsv) e.reserve(w.pre.size() + w.cmds.size());
    // ^ upper bound on ids/orders this workload can ever have. untimed, its setup like the preload
    feed(e, w.pre);
    auto t0 = std::chrono::steady_clock::now();
    o = feed(e, w.cmds);
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main(int argc, char** argv)
{
    size_t n = 10000;
    std::uint64_t seed = 1;
    int trials = 5;
    for(int i=1;i<argc;i++) {
        if(!std::strcmp(argv[i], "--n") && i+1<argc) n = std::strtoull(argv[++i], nullptr, 10);
        else if(!std::strcmp(argv[i], "--seed") && i+1<argc) seed = std::strtoull(argv[++i], nullptr, 10);
        else if(!std::strcmp(argv[i], "--trials") && i+1<argc) trials = std::atoi(argv[++i]);
        else if(!std::strcmp(argv[i], "--reserve")) g_rsv = true;
        else { std::fprintf(stderr, "usage: bench [--n N] [--seed S] [--trials T] [--reserve]\n"); return 2; }
    }
    if(n<2 || trials<1) { std::fprintf(stderr, "n>=2, trials>=1\n"); return 2; }

    std::vector<Workload> ws = all_workloads(n, seed);

    print_env();
    std::printf("n=%zu seed=%llu trials=%d (+1 warmup) reserve=%s\n", n, (unsigned long long)seed, trials, g_rsv ? "yes" : "no");
    std::printf("%-7s %8s %8s %16s %10s %10s %10s %12s %7s %6s %7s\n",
        "work", "pre", "cmds", "cksum", "median_ms", "min_ms", "max_ms", "cmds/s", "ok", "rej", "trades");

    std::uint64_t sink = 0;
    for(auto &w : ws) {
        MatchingEngine e;
        Out first, o;
        trial(e, w, first);   // warmup, thrown away

        std::vector<double> ms;
        for(int t=0;t<trials;t++) {
            ms.push_back(trial(e, w, o));
            if(!same(o, first)) { std::fprintf(stderr, "%s: trial %d gave different results??\n", w.name.c_str(), t); return 1; }
            sink += o.trades + (std::uint64_t)o.tradedQty;
        }
        std::sort(ms.begin(), ms.end());
        double med = (ms.size()%2) ? ms[ms.size()/2] : (ms[ms.size()/2-1] + ms[ms.size()/2]) / 2;
        // ^ even count -> avg of the middle two

        std::printf("%-7s %8zu %8zu %016llx %10.3f %10.3f %10.3f %12.0f %7zu %6zu %7zu\n",
            w.name.c_str(), w.pre.size(), w.cmds.size(), (unsigned long long)cksum(w),
            med, ms.front(), ms.back(), w.cmds.size() / (med/1000.0), o.ok, o.rej, o.trades);
    }
    std::printf("sink=%llu\n", (unsigned long long)sink);
    // ^ printing it so the compiler cant decide the work was pointless
    return 0;
}
