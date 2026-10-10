## log

- types, Order, book w/ map<px,list>, snapshot works
- fifo test is RED on purpose. match() is a stub
- buy side matching in, fifo test green

- sell side done. its a copy paste of the buy loop, clean up later maybe

- T02-T08 in, all green. M2 done i think

- MatchingEngine: owns book, seen set, seq counter. validates before touching anything
  seen = never reuse ids. NOT the same as "active", thats for cancel later

- cancel: linear scan of both sides, O(n orders). erase the level if it goes empty
  T01 T09-T13 green

- T16 bounds ok, UINT64_MAX id works

- check(): qty>0, no empty lvls, ids unique, fifo seq, not crossed. random 2000 ops seed 42 passes
  own LCG instead of std::uniform_int_distribution bc that differs between compilers

- conservation check: accepted = resting + cancelled + 2*traded, every step. random mix now has
  rejects too (qty 0 + reused ids) and those dont count

- T17: rejected adds/cancels dont touch book or seen. BAD_COMMAND one comes w/ the parser

- parser: tokens split on whitespace. number = -?[0-9]+ only, else BAD_COMMAND
  negative or too big for uint64 = INVALID_VALUE (its an integer, just not allowed)
  decided "+5" is BAD_COMMAND, from_chars doesnt take + and i dont want to either
  all tokens checked for shape first, then ranges -> precedence works out

- formatter: plain std::string building, no streams. example A matches byte for byte

- main wired up. stdin or --input file. stdout = results only, stderr = "line N: ERROR ..."
  exit 0 ok / 1 any reject / 2 usage or file problem

- fixtures A B C + .expected. ctest runs matcher via a cmake -P script (no bash/diff needed),
  checks --input and stdin give the same thing + exit code

- errors fixture through the cli, T18 replay (same seed twice + after reset = identical text)
  all T01-T18 covered now

- warnings in cmakelists now instead of me passing CXX_FLAGS by hand. clean build = 0 warnings

- -DSANITIZE=ON for asan+ubsan. full suite clean under it
  (sanitizers catch memory bugs/UB, they dont prove the matching is right - thats what tests are for)

- CI: github actions, ubuntu, debug + asan jobs, -Werror only there. no perf numbers in CI
  M4 done

- bench/: 4 workloads (rest, match, cancel, mixed), fixed seed, fnv checksum of the input
  preload kept separate from the timed cmds. no timing yet, just checks outcomes look sane

- timing: reset + preload outside the clock, steady_clock around the whole batch, 1 warmup,
  --trials (default 5), median/min/max. every trial has to give identical counts or it bails
  sink printed at the end so the work cant get optimised away

- bench prints compiler, flags, build type, commit, os, cpu. warns loudly on debug/asan builds
  build-release/ is its own dir so i dont mix them up

- baseline in docs/bench.md @ ead34ed. cancel 100k = ~97s (!!), linear scan is O(n) per cancel
  rest ~5M cmds/s, match ~3.8M cmds/s. mixed slow bc of its cancels

- buy/sell match loops -> one sweep() template. only reason its a template is the 2 map types
  tests + bench counts identical before/after

- cancel + check() deduped the same way. left snapshot() alone, its 5 lines

- index: unordered_map id -> {side, px, list iterator}, behind ENGINE_INDEX.
  engine = index (default), engine_scan = old scan. bench vs bench_scan same source
  entry gets dropped BEFORE the order is erased, on fill and on cancel
  cancel is O(log levels) now bc of the map find, not O(1)

- check() covers the index too: sizes match, every entry -> live order w/ same id/side/px
  tested by breaking it on purpose (not dropping the entry on fill) -> tests fail. exC follow up
  catches it first: CANCEL of a filled order finds the stale entry, then frees a dangling iterator and aborts

- tests_scan = same test file linked against engine_scan. ctest runs both (unit + unit_scan)

- before/after in docs/bench.md @ b9f5072. cancel 106s -> 33ms, mixed 13.5s -> 18ms
  BUT rest/match ~25% slower, the index isnt free. honest trade off, wrote it up

- workload gen / cksum / env print moved to bench/common.h so bench + bench_lat use the exact same inputs
  (checksums unchanged after the move)

- bench_lat: clock pair around every cmd, ns into a preallocated vector, 1 warmup + 5 trials pooled, nearest rank p50/p99/p99.9
  split by add/cancel. measures the clock first: empty now()->now() ~29ns p50, never saw a step < ~25ns, so thats the floor.
  raw numbers, overhead not subtracted. separate exe so nobody mixes it up with batch throughput
  index: adds ~250ns p50, cancel ~470-590ns p50. p50 repeats ok, p99 jumps up to 2x between runs (laptop + WSL2)

next: idx.reserve()?
