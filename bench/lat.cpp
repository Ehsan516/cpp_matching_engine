// per command latency. NOT the same thing as bench's batch numbers, a clock pair
// around every cmd is extra work the batch run doesnt do
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include "common.h"

using namespace bench;
using clk = std::chrono::steady_clock;
// using clk = std::chrono::high_resolution_clock;   thats system_clock on gcc, not steady. no

static std::int64_t ns(clk::time_point a, clk::time_point b) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}

// nearest rank. v has to be sorted already
static std::int64_t pct(const std::vector<std::int64_t>& v, double p) {
    size_t k = (size_t)(p * v.size());
    if(k*1.0 < p * v.size()) k++;   // ceil
    if(k==0) k=1;
    return v[k-1];
}

// how much the clock costs + how fine it is, so the real numbers can be read against it
static void calib(size_t reps)
{
    std::printf("clock:    steady_clock, is_steady=%d, nominal tick %.3f ns\n",
        (int)clk::is_steady, 1e9 * clk::period::num / clk::period::den);

    std::vector<std::int64_t> ov(reps);
    for(int pass=0; pass<2; pass++)   // 1st pass = warmup
        for(size_t i=0;i<reps;i++) {
            auto a = clk::now();
            auto b = clk::now();
            ov[i] = ns(a, b);
        }
    // ^ two now() back to back w/ nothing between, same shape as a sample. so ~what a free cmd would read as
    size_t zeros = (size_t)std::count(ov.begin(), ov.end(), 0);
    std::sort(ov.begin(), ov.end());
    std::printf("overhead: empty now()->now() pair, %zu samples: min %lld  p50 %lld  p99 %lld  max %lld ns  (%zu of them read 0 ns)\n",
        reps, (long long)ov.front(), (long long)pct(ov, 0.50), (long long)pct(ov, 0.99), (long long)ov.back(), zeros);

    // smallest step the clock actually moves. spin till it changes
    // cant come out lower than one now() call so its only an upper bound
    std::vector<std::int64_t> steps;
    for(size_t i=0;i<reps/100+1;i++) {
        auto a = clk::now(), b = a;
        while(b == a) b = clk::now();
        steps.push_back(ns(a, b));
    }
    std::sort(steps.begin(), steps.end());
    std::printf("resolution: smallest observed step %lld ns, median step %lld ns (%zu steps) - upper bound, limited by now() cost\n",
        (long long)steps.front(), (long long)pct(steps, 0.50), steps.size());
}

// reset + preload untimed, then a clock pair around each cmd
static Out trial(MatchingEngine& e, const Workload& w, std::vector<std::int64_t>& lat) {
    e.reset();
    feed(e, w.pre);
    Out o;
    for(size_t i=0;i<w.cmds.size();i++) {
        auto t0 = clk::now();
        apply(e, w.cmds[i], o);
        auto t1 = clk::now();
        lat[i] = ns(t0, t1);
        // ^ lat is allocated before the trial so this store is the only extra thing per cmd
    }
    return o;
}

static void printRow(const char* work, const char* op, std::vector<std::int64_t>& v,
                     const std::vector<std::int64_t>& p99s) {
    if(v.empty()) return;
    std::sort(v.begin(), v.end());
    double sum=0;
    for(auto x : v) sum += (double)x;
    auto lo = std::min_element(p99s.begin(), p99s.end());
    auto hi = std::max_element(p99s.begin(), p99s.end());
    std::printf("%-7s %-4s %9zu %7lld %7lld %9lld %9lld %11lld %9.0f %9lld %9lld\n",
        work, op, v.size(), (long long)v.front(), (long long)pct(v, 0.50), (long long)pct(v, 0.99),
        (long long)pct(v, 0.999), (long long)v.back(), sum / v.size(), (long long)*lo, (long long)*hi);
}

int main(int argc, char** argv)
{
    size_t n = 10000;
    std::uint64_t seed = 1;
    int trials = 5;
    std::string only;
    for(int i=1;i<argc;i++) {
        if(!std::strcmp(argv[i], "--n") && i+1<argc) n = std::strtoull(argv[++i], nullptr, 10);
        else if(!std::strcmp(argv[i], "--seed") && i+1<argc) seed = std::strtoull(argv[++i], nullptr, 10);
        else if(!std::strcmp(argv[i], "--trials") && i+1<argc) trials = std::atoi(argv[++i]);
        else if(!std::strcmp(argv[i], "--work") && i+1<argc) only = argv[++i];
        else { std::fprintf(stderr, "usage: bench_lat [--n N] [--seed S] [--trials T] [--work rest|match|cancel|mixed]\n"); return 2; }
    }
    if(n<2 || trials<1) { std::fprintf(stderr, "n>=2, trials>=1\n"); return 2; }

    std::vector<Workload> ws = all_workloads(n, seed);
    if(!only.empty()) {
        ws.erase(std::remove_if(ws.begin(), ws.end(), [&](const Workload& w) { return w.name != only; }), ws.end());
        if(ws.empty()) { std::fprintf(stderr, "no workload called %s\n", only.c_str()); return 2; }
    }

    print_env();
    std::printf("mode:     per command latency (clock pair around every add/cancel, NOT batch conditions)\n");
    calib(1000000);
    std::printf("n=%zu seed=%llu trials=%d (+1 warmup), percentiles pooled over all measured trials, nearest rank\n",
        n, (unsigned long long)seed, trials);
    std::printf("all times ns, raw (timer overhead NOT subtracted). p99 lo/hi = lowest/highest per trial p99\n");
    std::printf("%-7s %-4s %9s %7s %7s %9s %9s %11s %9s %9s %9s\n",
        "work", "op", "samples", "min", "p50", "p99", "p99.9", "max", "mean", "p99_lo", "p99_hi");

    std::uint64_t sink = 0;
    for(auto &w : ws) {
        MatchingEngine e;
        std::vector<std::int64_t> lat(w.cmds.size());
        Out first = trial(e, w, lat);   // warmup, thrown away

        std::vector<std::int64_t> all, adds, cxls;          // pooled over trials
        std::vector<std::int64_t> p99_all, p99_add, p99_cxl; // one per trial
        std::vector<std::int64_t> tAdd, tCxl;
        for(int t=0;t<trials;t++) {
            Out o = trial(e, w, lat);
            if(!same(o, first)) { std::fprintf(stderr, "%s: trial %d gave different results??\n", w.name.c_str(), t); return 1; }
            sink += o.trades + (std::uint64_t)o.tradedQty;

            tAdd.clear(); tCxl.clear();
            for(size_t i=0;i<lat.size();i++) (w.cmds[i].op==ADD ? tAdd : tCxl).push_back(lat[i]);
            std::vector<std::int64_t> tAll = lat;
            for(auto* v : {&tAll, &tAdd, &tCxl}) std::sort(v->begin(), v->end());
            p99_all.push_back(pct(tAll, 0.99));
            if(!tAdd.empty()) p99_add.push_back(pct(tAdd, 0.99));
            if(!tCxl.empty()) p99_cxl.push_back(pct(tCxl, 0.99));

            all.insert(all.end(), lat.begin(), lat.end());
            adds.insert(adds.end(), tAdd.begin(), tAdd.end());
            cxls.insert(cxls.end(), tCxl.begin(), tCxl.end());
        }
        const char* nm = w.name.c_str();
        if(!adds.empty() && !cxls.empty()) printRow(nm, "all", all, p99_all);
        // ^ only worth an "all" row if theres a mix
        printRow(nm, "add", adds, p99_add);
        printRow(nm, "cxl", cxls, p99_cxl);
        std::printf("%-7s      ok %zu  rej %zu  trades %zu  cksum %016llx\n",
            "", first.ok, first.rej, first.trades, (unsigned long long)cksum(w));
    }
    std::printf("sink=%llu\n", (unsigned long long)sink);
    return 0;
}
