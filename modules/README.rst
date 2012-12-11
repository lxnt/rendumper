This is DF modular backend bla-bla
----------------------------------



Notes
-----

mqueue - two questionable moments.
1. Timed wait will sleep for a multiple of timeout.
   See Timed Wait Semantics in man pthread_cond_wait

2. queue::pop() declares the queue writable when it 
   contains (max_messages - 1) messages and then pops
   another one. Dunno why I did it this way.

