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

next: formatter + wire into main
