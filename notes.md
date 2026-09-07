## log

- types, Order, book w/ map<px,list>, snapshot works
- fifo test is RED on purpose. match() is a stub
- buy side matching in, fifo test green

- sell side done. its a copy paste of the buy loop, clean up later maybe

- T02-T08 in, all green. M2 done i think

- MatchingEngine: owns book, seen set, seq counter. validates before touching anything
  seen = never reuse ids. NOT the same as "active", thats for cancel later

next: cancel by id (just scan the book for now, index later)
