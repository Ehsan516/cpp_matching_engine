# benchmarks

## setup

| | |
|---|---|
| commit | `d41e343` |
| compiler | GCC 15.2.0, `-O3 -DNDEBUG` (cmake Release), no sanitizers |
| os | Ubuntu on WSL2, kernel 6.6.114.1-microsoft-standard-WSL2, x86_64 |
| cpu | 12th Gen Intel Core i7-1255U (laptop, 12 logical cpus), ~9 GB ram visible to WSL |
| command | `./build-release/bench --n 100000 --seed 1 --trials 5` |

No special setup: no cpu pinning, no turbo/power control, other apps not closed down. so expect noise,
WSL2 adds some too.

### what is timed

- each trial: `reset()` the engine, apply the preload, **then** start `steady_clock`
- timed region = every `add()`/`cancel()` call in the workload, incl matching and building the trade vectors
- not timed: generating commands, parsing, formatting, printing
- 1 warmup trial thrown away, then 5 measured. each trial has to produce the same ok/rej/trade counts or the run aborts
- trade counts get summed into a `sink` that's printed at the end so the optimiser cant drop the work

### workloads

| name | preload (not timed) | timed commands | checksum (seed 1) |
|---|---|---|---|
| rest | none | 100k non-crossing adds, ~1000 prices each side | `c130ac011ad21cae` |
| match | 200k resting, 100k per side over 50 prices near the touch | 100k crossing adds, limit 1-10 ticks through, qty 1-150 | `1643c219b650097a` |
| cancel | 100k resting over ~1000 prices each side | cancel all 100k, shuffled order | `f2203f324900b231` |
| mixed | 10k resting over 100 prices each side | intended 50% passive add / 25% crossing add / 25% cancel random earlier id | `30c0a274aa91c7ce` |

## baseline: map&lt;px, list&gt; + linear scan cancel

| workload | median ms | min | max | cmds/s (median) | ok | rejected | trades |
|---|---|---|---|---|---|---|---|
| rest | 20.18 | 18.84 | 24.31 | ~4.95M | 100000 | 0 | 0 |
| match | 26.57 | 19.11 | 52.40 | ~3.76M | 100000 | 0 | 136874 |
| cancel | 97,214.86 | 92,538.88 | 103,964.32 | ~1,029 | 100000 | 0 | 0 |
| mixed | 11,844.55 | 11,774.65 | 11,999.84 | ~8,443 | 88894 | 11106 | 30466 |

mixed actual outcome: the 11106 rejects are all cancels that hit an id that had already filled or been cancelled
(`NOT_FOUND`). so the real mix isnt the intended one.

## reading it

- **cancel is the problem.** ~97s to cancel 100k orders. each cancel walks bids then asks level by level until
  it finds the id, so its O(resting orders) per cancel and O(n^2) for the whole workload. its ~5000x slower per
  command than adding
- mixed is slow for the same reason, only 25% of it is cancels but those dominate the time
- rest and match are fine for now. match max is 2x its median, one noisy trial, the min/median are close
- these are only for this machine. not trying to compare against real exchanges, and this is throughput of
  the in-memory engine, nothing to do with network latency

next: add an id -> (side, price, list iterator) index so cancel doesnt have to search. expected cost: more
memory and a bit of extra work on every add/fill to keep the index in sync. will measure the same 4 workloads after.

## after: id -> location index

`unordered_map<OrderId, {side, px, list iterator}>` for resting orders. entry added when an order rests, dropped
when it fills or gets cancelled. cancel = hash lookup + `map::find` for the level + list erase at a known spot,
so avg O(log levels) per cancel, not O(1) (hash lookup is average constant, not guaranteed).

both builds from the same commit (`b9f5072`, same release build dir, same machine/setup as above), run back to
back in one session: `bench` (index) first then `bench_scan` (scan). same command, n=100000 seed=1 trials=5.

> bench printed `b9f5072-dirty`. the only "change" was docs/bench.md having CRLF line endings in the checkout
> (WSL git doesnt convert them), no source file differed.

| workload | scan median ms | index median ms | change | index min / max |
|---|---|---|---|---|
| rest | 20.01 | 24.97 | **+24.8%** (slower) | 22.07 / 26.55 |
| match | 19.95 | 25.50 | **+27.8%** (slower) | 23.49 / 28.45 |
| cancel | 105,929.39 | 32.68 | **-99.97%** (~3,240x faster) | 31.27 / 34.50 |
| mixed | 13,542.15 | 18.34 | **-99.86%** (~740x faster) | 17.22 / 19.23 |

outcomes are identical between the two (same checksums, ok/rej/trade counts, sink=28418420) so theyre doing
the same work. unit tests run against both builds too (`unit` / `unit_scan`).

### reading it

- cancel goes from ~1k to ~3M cmds/s. the whole O(n^2) thing is gone, its now about the same cost as an add
- **adds and matching got ~25-28% slower.** every resting add now also does a hash insert (and sometimes a
  rehash), every filled maker does a hash erase. in rest and match thats pure overhead since nothing gets cancelled
- so its a trade: if the workload cancels basically never, the scan version is faster. any real amount of
  cancelling and the index wins by a mile (mixed is only 25% cancels and its 740x faster)
- memory: the index holds one entry per resting order. didnt measure how much extra that is, so not claiming a number
- noise: the scan cancel median was 97.2s in the baseline run and 105.9s here, max 146.7s this time.
  same code for that part, so session to session variation is easily ~10% on this laptop. the 25% add slowdown is
  bigger than that and both columns came from the same session, but treat it as roughly 25%, not exactly
- possible next thing (not doing it now): `idx.reserve()` up front to skip rehashing, might win back some of the add cost

## per command latency

separate exe, `bench_lat` (+ `bench_lat_scan`). **not the same numbers as the batch tables above, dont convert one
into the other.** a clock pair around every command is extra work the batch run doesnt do.

```
./build-release/bench_lat --n 100000 --seed 1 --trials 5
```

### what is timed

- same workloads as `bench`. generators moved into `bench/common.h`, both exes use it. checksums are the same
  (`c130ac01..`, `1643c219..`, `f2203f32..`, `30c0a274..`) and so are ok/rej/trades and sink
- same as batch: `reset()` + preload before timing, 1 warmup thrown away, 5 measured, every trial has to give the same counts
- per command: `t0 = steady_clock::now()`, the engine call (incl reading + freeing the result, same per cmd work as
  the batch loop), `t1 = now()`, then `t1-t0` in ns into a vector allocated before the trial
- extra per command vs batch: 2 clock reads + one 8 byte store. the store is outside t0..t1 but still uses cache
  (100k samples = 800 KB per trial)
- percentiles = nearest rank over all 5 trials pooled (500k samples per workload). `p99 lo/hi` = lowest/highest
  p99 of the single trials, to show the spread
- **raw numbers, timer overhead NOT subtracted**

### the timer

`std::chrono::steady_clock` (libstdc++ = `clock_gettime(CLOCK_MONOTONIC)` underneath). is_steady=1, nominal tick 1 ns.
bench_lat measures it at startup:

| | run A | run B |
|---|---|---|
| overhead, empty now() -> now() pair, 1M samples, min / p50 / p99 | 24 / 29 / 45 ns | 25 / 29 / 46 ns |
| overhead max | 215 us | 477 us |
| samples that read 0 | 0 | 0 |
| smallest / median step the clock was seen to move | 25 / 29 ns | 29 / 32 ns |

- so every sample below has ~25-30 ns of clock in it. thats ~10% at p50 (~250 ns), nothing at p99
- nominal tick is 1 ns but i only ever saw it move in steps about one `now()` call big. that measurement cant go below
  the cost of a call, so real resolution is <= ~25 ns. anything under ~30 ns isnt resolvable here
- the overhead max (100s of us with **nothing** being timed) is the machine (interrupts, scheduler, WSL2), not the
  engine. same goes for the max column below

### results: id index build

same machine/os/compiler as above. built from `ae95a80` + the uncommitted bench changes (prints `ae95a80-dirty`,
engine sources unchanged). 3 runs back to back, n=100000 seed=1 trials=5. run A's binary only differed in two printf strings.

| workload | op | samples/run | p50 ns (A / B / C) | p99 ns (A / B / C) | p99.9 ns (A / B / C) | min ns |
|---|---|---|---|---|---|---|
| rest | add | 500k | 251 / 265 / 252 | 859 / 1204 / 579 | 3029 / 4518 / 1642 | 105 |
| match | add | 500k | 218 / 232 / 248 | 1532 / 2013 / 1399 | 7435 / 7882 / 5763 | 92 |
| cancel | cancel | 500k | 494 / 469 / 593 | 2722 / 2407 / 4060 | 10574 / 7549 / 24815 | 114 |
| mixed | all | 500k | 236 / 262 / 241 | 1348 / 3401 / 2048 | 4934 / 9031 / 6138 | 41 |
| mixed | add | 375,255 | 234 / 254 / 238 | 986 / 2332 / 1483 | 3416 / 5867 / 4598 | 93 |
| mixed | cancel | 124,745 | 257 / 347 / 262 | 2651 / 5244 / 3289 | 7411 / 14531 / 8111 | 41 |

max per run was 0.27-7.7 ms, see the timer bit for why im not reading anything into it.

run B per trial p99 lo/hi, to show the spread inside one run: rest 850/1427, match 1832/2250, cancel 1914/2870,
mixed 2703/4038.

### reading it

- **p50 is fairly stable, p99 isnt.** add p50 moves <= ~15% between runs, cancel p50 more (469-593, and 257-347 in
  mixed). p99 moves up to 2x (rest 579-1204, mixed cancel 2651-5244) and a lot between trials of one run too. on a
  laptop + WSL2 the tail is as much the environment as the engine, so i wouldnt rank workloads by p99 unless the gap is big
- adds are ~220-270 ns p50 whether resting or crossing. match has the fatter tail (p99 1.4-2 us vs 0.6-1.2 us for rest).
  guess: sweeps through several levels + more trade vector growth. not profiled
- cancel in the cancel workload is ~2x an add at p50 (~470-590 ns). guess: cache misses from cancelling shuffled ids
  in a 100k order book (hash lookup + map find + list node all in different places). also not profiled.
  in mixed (smaller book, ~9% of cancels miss) cancel p50 is ~260-350 ns. the 41 ns min is a `NOT_FOUND`
- instrumented mean vs batch, same session: batch gave 311 / 350 / 536 / 241 ns per cmd (rest / match / cancel / mixed).
  bench_lat means over the 3 runs were 320-399 / 341-373 / 576-821 / 357-511 ns. so timed runs come out at or above
  batch, about what youd expect from a clock pair per cmd + noise. mixed is the furthest apart and i dont have a good
  reason why. thats why the two arent reported as the same thing
- noise: this session's batch run was slower than the "after" table (cancel 53.6 vs 32.7 ms median, rest 31.1 vs 25.0).
  same code so its the laptop. compare the latency numbers with each other, not with the older batch tables

## reserve() experiment

the index made adds ~25% slower. idea: part of that is the hash tables (`seen` + `idx`) rehashing as they grow,
so size them up front. `MatchingEngine::reserve(n)` reserves both, `bench --reserve` calls it right after `reset()`
with n = preload + timed commands (upper bound on ids/orders), outside the timed part.

also found while doing this: `reset()` used `seen.clear()`, which keeps the bucket array. so in all the batch tables
above, every trial after the warmup started with `seen` already big enough and only `idx` had to grow. fixed in
`d2ab324` (`seen` = new empty set). the numbers below have the fixed reset, so dont compare them with the older tables.

### setup

commit `e857342` (clean), index build, release, same laptop/WSL2 as above. n=100000 seed=1 trials=5.
8 runs alternating: A1 B1 A2 B2 then B3 A3 B4 A4 (A = no reserve, B = `--reserve`), reserve first in the second
half so a drift over time cant favour one side. outcomes identical in every run (checksums, counts, sink=28418420).

**this session was noisy.** everything is slower than the earlier sessions (rest ~25 ms then, 37-50 ms now without
reserve) and the same config moved a lot between runs (cancel without reserve: 51 / 100 / 62 / 72 ms).

### results (median ms per run)

| workload | no reserve A1 / A2 / A3 / A4 | reserve B1 / B2 / B3 / B4 | median of the 4 runs, no -> yes | reserve faster in |
|---|---|---|---|---|
| rest | 37.8 / 45.8 / 37.1 / 50.5 | 25.7 / 32.5 / 30.0 / 26.8 | 41.8 -> 28.4 (~ -32%) | 4 of 4 pairs |
| match | 42.0 / 46.3 / 39.2 / 35.1 | 35.9 / 40.0 / 38.7 / 37.7 | 40.6 -> 38.2 (~ -6%) | 3 of 4 |
| cancel | 51.3 / 99.9 / 61.5 / 72.0 | 64.3 / 74.2 / 61.3 / 69.2 | 66.75 -> 66.75 (0) | 3 of 4 |
| mixed | 25.2 / 31.9 / 26.5 / 29.3 | 22.5 / 25.1 / 22.3 / 23.9 | 27.9 -> 23.2 (~ -17%) | 4 of 4 |

### reading it

- lines up with where inserts happen inside the timed part:
  - rest: 100k new ids into both tables from empty -> lots of rehashing -> biggest win, and it won every pair
  - mixed: ~75k adds timed, preload is small -> clear win, every pair
  - match: 200k of the 300k ids go in during the preload (untimed) so the tables are mostly grown already.
    3 of 4 pairs but one of those is 38.7 vs 39.2, and the run to run spread is way bigger than 6%. maybe a small
    win, not calling it
  - cancel: nothing gets inserted while timing -> no change expected. medians come out exactly equal, the 3 of 4
    is just noise (one pair is 61.3 vs 61.5)
- so rehashing was a real chunk of the add cost. didnt rerun `bench_scan` in this session, so not claiming
  how much of the index's ~25% add penalty this wins back, only that reserve helps adds vs not reserving
- the catch: the engine doesnt know n. a real system would size it from expected order count at startup.
  so `reserve()` stays optional, default behaviour unchanged
- given the noise, i'd trust the direction for rest/mixed, not the exact percentages
