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

next: T17 atomicity test, then start on the parser
