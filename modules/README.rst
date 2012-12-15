This is DF modular backend bla-bla
----------------------------------



Notes
-----

1. Mqueue timed wait will sleep for a multiple of timeout.
   See Timed Wait Semantics in man pthread_cond_wait

2. queue::pop() declares the queue writable when it
   contains (max_messages - 1) messages and then pops
   another one. Dunno why I did it this way.

3. Code is not properly reused between renderers yet.
   Got to have a couple written first to see what can be done.

4. Renderer control - zoom, scroll, etc. Resize is already
   handled internally, why not the rest of it?

5. Lots of stuff prone to failure in constructors.

6. I compile 32bit SDL using the --host=i686-linux-gnu flag.

7. get/set_gridsize do the opposite of what they should.
   And, why are they needed at all?

8. do something so that if render thread can't give a df_buffer_t,
   simulation thread will not hang on that.
