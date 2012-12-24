This is DF modular backend bla-bla
----------------------------------

Todo list
---------

In no particular order.

- logging initialization / config file
- renderer/simuloop config api its use with initst and a file for tests.
- decide if data races are really wanted. move everything to mqueues if not.
- clean up itc_message_t
- move emafilter.h to common code.
- see if libgraphics.so still builds.
- do the TTF support
- offload  dim/rain/snow effects to the renderer, implement in shaders,
  reimplement original in the ncurses renderer
- compile with the same flags as SDL is: -mmx -3dnow -sse
- do a knob for max-optimization builds.
- show fps+averaged times on an overlay. maybe do a graph ala eve online
- rewrite df-structures/codegen.py, it rotted; revive

Starting up, paths and stuff
----------------------------

Mode of operation is: program binary (df, etc), calls ``glue.cpp::lock_and_load()``.
It expects renderer name and module path.

Default module path shall be ``libs/`` for fortress mode,
and ``<prefix-when-built>/lib/df-modules`` for tests mode.
It is all hardcoded into load_platform()'s callers.

Default shader source path shall be ``data/shaders`` for fortress mode, and ``something else``
for tests mode. Shaders are embedded anyway, so this is not strictly required.

Renderer name shall be accepted from the command line or from an environment variable ``DF_PRINTMODE``.
Platform name is inferred from the renderer name in the loader.
This is because platform implementation must be loaded before init.txt
or any other configuration can be read; and each renderer requires strictly certain platform.

Logging configuration file is logging.properties.
It is searched for in current directory and data/init or ``<prefix-when-built>/etc/df-modules``.

Simuloop, sound and renderer options.
These are - fps caps, sound module name and enable/disable, renderer initial font,
shaders path, preferred window size, (fullscreen ?), color map - overriden in fortress mode,
whatever else.

Configuration access is via iplatform interface, ``const char *getprop(const char *name)`` method.

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

6. I compile linux 32bit SDL using the --host=i686-linux-gnu flag.
   Similar with win32. --build doesn't work.

7. get/set_gridsize do the opposite of what they should.
   And, why are they needed at all?

8. logging: getlogr and logconf are slow by design.
   getplatform()->getlogr(some-random-name)->trace() inside a loop
   is a very bad practice. don't do it.


mingw-w64 build
---------------

Build script needs writing. For now I'll just leave this here::

    lxnt@bigbox:~/00DFGL/build-win32/sdl2$ ../../fgtestbed/deps/SDL/configure --host=i686-w64-mingw32 --prefix=/home/lxnt/00DFGL/prefix-win32/
    make -j 4
    make install

    http://code.google.com/p/zlib-mingw32/downloads/list
    tar jxf ../zlib-1.2.3-mingw32bin.tar.bz2
    cd zlib-1.2.3/
    cp lib/* ~/00DFGL/prefix-win32/lib/
    cp include/* ~/00DFGL/prefix-win32/include/
    cp bin/*.dll  ~/00DFGL/prefix-win32/bin/

    lxnt@bigbox:~/00DFGL/build-win32/sdl_pnglite$ cmake -DCMAKE_TOOLCHAIN_FILE=../w32tc.cmake -DCMAKE_INSTALL_PREFIX=/home/lxnt/00DFGL/prefix-win32/ ~/projects/SDL_pnglite/

    get glew-1.9.0 source - http://glew.sf.net/

     i686-w64-mingw32-ld -o lib/glew32.dll src/glew.o -L/mingw/lib -lglu32 -lopengl32 -lgdi32 -luser32 -lkernel32
     i686-w64-mingw32-gcc -o lib/glew32.dll src/glew.o -shared -Wl,-soname,glew32.dll -Wl,--out-implib,lib/libglew32.dll.a  -lglu32 -lopengl32 -lgdi32 -luser32 -lkernel32
    cp lib/libglew32.dll.a ../../prefix-win32/lib/
    cp include/GL/* ../../prefix-win32/include/GL/
    cp lib/glew32.dll  ../../prefix-win32/bin/

    CFLAGS=-I/home/lxnt/00DFGL/prefix-win32/include/ cmake -DCMAKE_TOOLCHAIN_FILE=../w32tc.cmake -DCMAKE_INSTALL_PREFIX=/home/lxnt/00DFGL/prefix-win32/ ~/00DFGL/rendumper/modules/
    make
    make install

    cd /home/lxnt/00DFGL/prefix-win32/
    wine test-life.exe sdl2gl2
