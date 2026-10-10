# wanderwall

Single-threaded, in-memory limit order matching engine in C++17, for one made-up instrument.
Price-time priority, partial fills, cancel by id, aggregated book snapshots. Learning project, not a real exchange.

## build

needs cmake + a C++17 compiler (tested with g++ on Ubuntu / WSL)

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

sanitizer build (asan + ubsan):

```
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure
```

## benchmark

separate release build, dont benchmark the debug or asan ones:

```
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/bench --n 100000 --seed 1 --trials 5
```

prints compiler / flags / commit / cpu first so results can be traced back. results + method in [docs/bench.md](docs/bench.md).

## run

```
./build/matcher < fixtures/example.txt
./build/matcher --input fixtures/example.txt
```

both give the same stdout. one command per line:

| command | |
|---|---|
| `BUY <id> <price> <qty>` | buy limit order |
| `SELL <id> <price> <qty>` | sell limit order |
| `CANCEL <id>` | cancel whatever is left of an active order |
| `BOOK` | print aggregated book |

Keywords are uppercase only. Blank lines and lines starting with `#` are skipped. No inline comments.

### output

- accepted order: `ACK <id>`, then `TRADE <maker> <taker> <price> <qty>` per fill, then `REST <id> <left>` or `DONE <id>`
- cancel: `CANCELLED <id> <qty>` or `ERROR NOT_FOUND`
- rejects: `ERROR BAD_COMMAND` (unknown verb, wrong number of fields, not an integer), `ERROR INVALID_VALUE` (integer but out of range), `ERROR DUPLICATE_ID`
- `BOOK`: `BOOK_BEGIN`, bids high to low as `BID <px> <qty>`, asks low to high as `ASK <px> <qty>`, `BOOK_END`

Line numbers for rejected commands go to stderr, stdout is only results.

Exit code: 0 everything accepted, 1 at least one reject, 2 bad arguments / cant open file.

## rules

- prices are integer ticks, 1 to 1,000,000,000. qty 1 to 1,000,000,000. ids are positive uint64
- trades always happen at the **resting** order's price
- same price -> oldest first. a partially filled resting order keeps its place
- ids can't be reused ever (in one process), even after the order fills or is cancelled. rejected orders don't use up their id
- a rejected command changes nothing

## not included

market orders, amendments, time in force, fees/accounts, self trade prevention, networking, threads, persistence. one instrument only.

## layout

```
include/engine/   types, book, engine, parser, formatter headers
src/              implementation
app/main.cpp      cli
tests/            unit tests + fixture runner
fixtures/         example inputs + expected outputs
```

## todo

- ~~benchmarks (bench/) + write up~~
- ~~cancel is a linear scan right now, want to try an id -> location index~~ done, see docs/bench.md.
  cancel went from ~100s to ~33ms for 100k cancels, adds got ~25% slower. old scan version still builds as `engine_scan`
- try `reserve()` on the index
