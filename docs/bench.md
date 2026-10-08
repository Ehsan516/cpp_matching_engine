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
