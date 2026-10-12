# design notes / decision log

format for each one: problem, options, what i picked, why, evidence. the day to day log is in notes.md, this is the
cleaned up version.

## 1. integer ticks for prices

- **problem:** prices need exact comparison (crossing, sorting levels, equal price = same level)
- **options:** `double`, decimal type, integer ticks
- **picked:** `int64_t` ticks, 1 tick = 0.01 in the examples. qty is `int64_t` too
- **why:** doubles can't represent 0.01 exactly, so two "equal" prices can end up on different levels or fail to
  cross. integers compare exactly and are cheap map keys. signed so subtracting quantities can't silently wrap
- **evidence:** limits are 1..1e9 for both, so even 5 orders of max qty at one level (5e9) fit fine, tested in T16.
  level totals use checked addition (`add_chk`) anyway

## 2. trade at the resting order's price

- **problem:** buy limit 10005 meets sell resting at 10004 (or the other way). which price?
- **options:** incoming price, resting price, midpoint
- **picked:** always the resting (maker) price
- **why:** the maker was there first and showed that price. the taker said "up to my limit", so it gets anything
  better for free (price improvement). matches the PDD's contract
- **evidence:** example C fixture + T08 both directions. sell limited at 10004 trades at 10005

## 3. seen ids vs active ids

- **problem:** ids must never be reused, and cancel needs to find live orders
- **options:** one set for both, or two structures
- **picked:** two. `MatchingEngine::seen` = every accepted id ever (incl filled/cancelled), only shrinks on `reset()`.
  `OrderBook::idx` (index build) = only resting orders, id -> where it sits
- **why:** they answer different questions. "has this id ever been used" vs "where is this order right now". if
  one set did both, a filled order would either block reuse correctly but break cancel lookups, or the other way round
- **evidence:** T12 (active/filled/cancelled ids all rejected), example C follow up (`CANCEL 31` -> NOT_FOUND but
  `BUY 31` -> DUPLICATE_ID), rejected adds don't reserve their id (eng_invalid, T17)

## 4. book structure: map<price, list<Order>>

- **problem:** need best price fast, price order for snapshots, FIFO inside a level, and removing from the middle
- **options:** sorted vector of orders, map of levels w/ deque or list, heap, flat arrays indexed by price
- **picked:** `std::map<Px, std::list<Order>>` per side, bids with `std::greater` so `begin()` is always best on both sides
- **why:** easiest one to get right. map keeps levels sorted and `begin()` is the best price. list keeps FIFO and
  erasing a known node is O(1) and doesnt move anything else, which is what makes the index safe (see 6)
- **cost:** every new level = a map node allocation, every resting order = a list node allocation. pointer chasing,
  bad locality. not claiming this is fast, its the baseline
- **evidence:** baseline numbers in bench.md, rest/match ~4-5M cmds/s on this laptop

## 5. cancel: linear scan -> id index

- **problem:** baseline cancel walks the book until it finds the id. 100k cancels on a 100k book = ~97-106 s
- **options:** keep scanning, store a locator per resting order, intrusive lists, flat arrays
- **picked:** `unordered_map<OrderId, Loc{side, px, list iterator}>`, behind `ENGINE_INDEX`. old scan still builds
  as `engine_scan` so the two can be compared
- **why:** smallest change that removes the O(n) search
- **cost:** extra memory per resting order (not measured), hash insert on every rest, hash erase on every fill/cancel
- **evidence:** cancel ~3240x faster, mixed ~740x faster, rest/match ~25-28% slower. bench.md "after" section.
  both builds give identical outcomes + both pass the full test suite (`unit`, `unit_scan`)

## 6. lifetime / ownership

- orders are owned by value by the list at their price level, which is owned by the side's map, which is owned by
  `OrderBook`, owned by `MatchingEngine`. no `new`/`delete` anywhere, everything is RAII
- `Loc::it` is a non owning handle. a `std::list` iterator stays valid when *other* elements are inserted/erased, and
  the list object itself never moves because map nodes dont move. so the only thing that kills it is erasing that
  exact order, which happens in exactly two places: a fill (`sweep`) and a cancel. both drop the index entry first
- a level gets erased from the map only when its list is empty, so no iterator can point into it at that point
- `unordered_map` rehash invalidates iterators *into the hash map*, but not the `Loc` values stored in it, so the
  list iterators inside are fine
- `reset()` replaces the whole book, every handle goes with it
- `check()` verifies index <-> book both ways after every step in the random test. breaking it on purpose (not
  dropping the entry on fill) makes the tests fail + abort on the dangling iterator

## 7. invalid commands

- **problem:** what happens to state when a command is bad
- **picked:** validate everything first, change nothing on reject. order of checks: command shape + numeric
  parsing (`BAD_COMMAND`), then ranges (`INVALID_VALUE`), then duplicate id (`DUPLICATE_ID`)
- **why:** one predictable error for ambiguous input, and a reject can never leave half an order behind
- details: numbers are `-?[0-9]+` only, converted with `std::from_chars` so overflow is caught. negatives and too big
  for uint64 are `INVALID_VALUE` (they are integers, just not allowed). `+5`, `1.5`, `12abc`, `0x10` are `BAD_COMMAND`
- **evidence:** T14, T15, T17, `fixtures/errors.txt`
- **known limitation:** allocation failure isnt handled. `add()` puts the id into `seen` before matching, so if a
  `bad_alloc` came out of matching the id would be burned with nothing in the book. version 1 doesnt try to recover
  from that, documented instead

## 8. output format

- one line per event, plain text, machine readable on stdout. line numbers for rejects go to stderr
- `ACK`, then `TRADE maker taker px qty` in the order they happened, then exactly one of `REST id left` / `DONE id`.
  `DONE` only for the incoming order, makers dont get their own completion line
- `BOOK` never iterates an unordered container, it comes straight from the maps so its always bids high->low then
  asks low->high
- formatter is separate from the engine, engine returns structs (`AddRes`, `CxlRes`, `Lvl`), formatter turns them into
  strings. so engine tests dont depend on text, fixture tests check the exact text
- stdin and `--input` go through the same `run()`, fixture runner checks they match

## 9. benchmark boundaries

- timed: engine `add()`/`cancel()` calls incl matching and building the result vectors
- not timed: generating commands, preload, `reset()`, parsing, formatting, printing
- fixed seed + own LCG (std distributions arent the same across standard libraries), fnv checksum of every workload
- warmup + 5 trials, median, all trials must give the same counts, result `sink` printed so nothing gets optimised away
- release build only, separate `build-release/`. bench prints compiler/flags/commit/os/cpu
- per command latency is a separate exe (`bench_lat`) because the clock calls change the conditions
- no perf numbers in CI, shared runners are too noisy

## 10. single threaded

one instrument, one thread, commands processed strictly in order. the book has no locks and doesnt need any.
determinism (same input -> same output, T18) comes for free. making it multi threaded would mean deciding who owns
the book and how commands get ordered, which is a different project. for multiple instruments the easy version is
one `MatchingEngine` per symbol (`map<symbol, MatchingEngine>`), seen ids either per symbol or global depending on the rules

## complexity (index build, what the code actually does)

L = price levels on a side, N = resting orders, k = makers touched by one incoming order

| op | cost | allocates |
|---|---|---|
| add, rests without trading | O(log L) map lookup/insert + O(1) list push + avg O(1) hash insert x2 (seen + idx) | new level (map node) if its a new price, list node, seen node, idx node. no trades = empty vector, no allocation |
| add that matches | O(k + levels emptied) (best level is `begin()`, erasing it is amortized O(1)) + the resting cost above if something is left | trade vector (grows as trades are added), + above if it rests |
| cancel (index) | avg O(1) hash + O(log L) map find + O(1) list erase | nothing (frees) |
| cancel (scan) | O(N) | nothing (frees) |
| book snapshot | O(N) sums every order, totals arent cached | result vector |

hash lookups are average O(1), not guaranteed. `seen` grows forever (until reset), one node per accepted order.
