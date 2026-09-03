## log

- types, Order, book w/ map<px,list>, snapshot works
- fifo test is RED on purpose. match() is a stub

next: write the matching loop for incoming buy (walk asks low->high), then sell side.
dont forget trade px = resting px
