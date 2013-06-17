This is DF modular backend
**************************

Todo list
---------

Missing features - priority:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- TTF support - convert addst/addcoloredst/whatever else that can be to just submitting
  strings, positions and attributes along with the df_buffer_t and let the renderer
  sort'em out. Have to write something to the screen and advance pointers there though.
- Decide what to do with 2D world map drawing and export
  (currently it's SDL_SaveBMP buried in the binary).
  Well, got to have a glimpse on what API is used to draw this. Otherwise it's just
  reimplementing rgba32 SDL_Surface, and this ain't no fun.

In no particular order:
^^^^^^^^^^^^^^^^^^^^^^^

- fix SDL mouse input: continuous drawing does not work for some reason
- renderer/simuloop config api its use with initst and a file for tests.
- decide if data races are really wanted. move everything to mqueues if not.
- clean up itc_message_t
- offload  dim/rain/snow effects to the renderer, implement in shaders,
  reimplement original in the ncurses renderer.
- compile with the same flags as SDL is: -mmx -3dnow -sse
- make a knob for max-optimization builds.
- show fps+averaged times on an overlay. maybe do a graph ala eve online
- rewrite df-structures/codegen.py, it rotted; revive
- "ld.so understands the string $ORIGIN (or equivalently ${ORIGIN}) in
  an rpath specification (DT_RPATH or DT_RUNPATH) to mean the directory
  containing the application executable."
- sdl2gl* renderers - move common code to common/
- maybe get rid of vbstreamer in sdl2gl2.
- merge ui and compositor from fgtestbed. first finish and debug it though
- ncurses mouse input
- sdl2gl2 GL_POINT positioning suffers rounding errors: eats pixels.
  visible with DF_LOG=sdl.reshape=trace.

Stuff I'd like to GSoC off:
^^^^^^^^^^^^^^^^^^^^^^^^^^^

- pure SDL2 renderer ala PRINT_MODE:2D
- pure SDL2 sound module, if it's possible
- OpenAL sound module
- Offscreen ncurses renderer complete with finding the appropriate file
  format and viewer
- i686-apple-darwin10 build

Starting up, paths and stuff
----------------------------

Quickstart
^^^^^^^^^^

Build modules and the libgraphics.so, install into some prefix;
link all .so-s into DF's libs/ like this::

    lrwxrwxrwx 1 lxnt lxnt       37 Dec 31 16:38 common_code.so -> /tmp/prefix/lib/dfmodules/common_code.so
    -rwxr-xr-x 1 lxnt lxnt 15104448 Jun  4  2012 Dwarf_Fortress
    -rw-r--r-- 1 lxnt lxnt   466491 Jun  4  2012 libgcc_s.so.1.orig
    lrwxrwxrwx 1 lxnt lxnt       27 Dec 31 16:38 libgraphics.so -> /tmp/prefix/lib/libgraphics.so
    -rwxr-xr-x 1 lxnt lxnt  1451966 Jun  4  2012 libgraphics.so.orig
    lrwxrwxrwx 1 lxnt lxnt       29 Dec 31 16:39 libSDL-1.2.so.0 -> /tmp/prefix/lib/libSDL2-2.0.so.0
    -rwxr-xr-x 1 lxnt lxnt  4852343 Jun  4  2012 libstdc++.so.6.orig
    lrwxrwxrwx 1 lxnt lxnt       39 Dec 31 16:38 platform_sdl2.so -> /tmp/prefix/lib/dfmodules/platform_sdl2.so
    lrwxrwxrwx 1 lxnt lxnt       42 Dec 31 16:38 renderer_sdl2gl2.so -> /tmp/prefix/lib/dfmodules/renderer_sdl2gl2.so

Notice renamed libgcc_s.so.1 and libstdc++.so.6.

Launch as usual.


Mode of operation is:
^^^^^^^^^^^^^^^^^^^^^

Program binary (df, etc), calls ``glue.cpp::lock_and_load()``.
It expects renderer name and module path.

Default module path is set up at configure time, and by even more default is
``<install-prefix>/lib/dfmodules``

Module path of ``libs/`` is hardcoded into the libgraphics.so.

Default shader source path shall be ``data/shaders`` for libgraphics.so, and ``something else``
for the tests. Shaders are embedded anyway, so this is highly optional. They get searched for there before
using embedded versions though.

Renderer name shall be accepted from the command line or from an environment variable ``DF_PRINTMODE``.
Platform name is inferred from the renderer name in the loader.
This is because platform implementation must be loaded before init.txt
or any other configuration can be read; and each renderer requires strictly certain platform.

Logging configuration is the DF_LOG environment variable.

Format is loggername=level,logger2=level[,....]

Beware: setting root=trace will dump texalbum and dump vertex buffers as they are drawn,
plus generate a dozen lines of timing trace per frame.
Use with care, best on a tmpfs mount with output redirected.

Simuloop, sound and renderer options.

These are - fps caps, sound module name and enable/disable, renderer initial font,
shaders path, preferred window size, (fullscreen ?), color map, whatever else.

Configuration access is via iplatform interface, ``const char *getprop(const char *name)`` method (TBD).

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
   getplatform()->getlogr(some-varying-name)->trace() inside a loop
   is a very bad practice. don't do it.

9. gps_locator kills any hope for write-only df_buffer_t-s.
   Also fx code with its \|=-s isn't very helpful.
   And I'm afraid it all is going to have to move to uint32_t
   writes before it can go write-only. Can as well move to
   row-major interleaved then. Btw, gps_locator seem to be used
   only for FPS display, might be possible to get rid of it.


Building this:
--------------

i686-linux-gnu build
^^^^^^^^^^^^^^^^^^^^

For both native and crosscompile from x86-64 host.

Use init-prefix.sh::

    mkdir /tmp/prefix ../build ; cd ../build ; ../rendumper/init-prefix.sh ../build /tmp/prefix

Then::

    mkdir rd-build; cd rd-build

    ccmake -DCMAKE_TOOLCHAIN_FILE=../../rendumper/gcc-4.5.cmake -DCMAKE_INSTALL_PREFIX=/tmp/prefix ../../rendumper

    make && make install


i686-w64-mingw32 build
^^^^^^^^^^^^^^^^^^^^^^

Build script needs writing. For now I'll just leave this here::

    get latest from http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Automated%20Builds/
    point PATH there
    fix path in w64-mingw32-gcc-4.8.cmake

    lxnt@bigbox:~/00DFGL/build-win32/sdl2$ ../../fgtestbed/deps/SDL/configure --host=i686-w64-mingw32 --prefix=/home/lxnt/00DFGL/prefix-win32/
    make -j 4
    make install

    http://code.google.com/p/zlib-mingw32/downloads/list
    tar jxf ../zlib-1.2.3-mingw32bin.tar.bz2
    cd zlib-1.2.3/
    cp lib/* ~/00DFGL/prefix-win32/lib/
    cp include/* ~/00DFGL/prefix-win32/include/
    cp bin/*.dll  ~/00DFGL/prefix-win32/bin/

    lxnt@bigbox:~/00DFGL/build-win32/sdl_pnglite$ cmake -DCMAKE_TOOLCHAIN_FILE=~/00DFGL/rendumper/w64-mingw32-gcc-4.8.cmake -DCMAKE_INSTALL_PREFIX=/home/lxnt/00DFGL/prefix-win32/ ~/projects/SDL_pnglite/

    get glew-1.9.0 source - http://glew.sf.net/

    i686-w64-mingw32-gcc -DGLEW_NO_GLU -O2 -Wall -W -Iinclude -DGLEW_BUILD -DSTATIC -o src/glew.o -c src/glew.c
    i686-w64-mingw32-gcc -o lib/glew32.dll src/glew.o -shared -Wl,-soname,glew32.dll -Wl,--out-implib,lib/libglew32.dll.a  -lglu32 -lopengl32 -lgdi32 -luser32 -lkernel32
    cp lib/libglew32.dll.a ../../prefix-win32/lib/
    cp include/GL/* ../../prefix-win32/include/GL/
    cp lib/glew32.dll  ../../prefix-win32/bin/

    CFLAGS=-I/home/lxnt/00DFGL/prefix-win32/include/ cmake -DCMAKE_TOOLCHAIN_FILE=../w32tc.cmake -DCMAKE_INSTALL_PREFIX=/home/lxnt/00DFGL/prefix-win32/ ~/00DFGL/rendumper/modules/
    make
    make install

    cd /home/lxnt/00DFGL/prefix-win32/
    wine test-life.exe sdl2gl2


MSVC build
^^^^^^^^^^

Use VS Express 2010. Other versions were not tested.

Use cmake-gui.

Building modules has not been tested, probably needs additional
support in CMakeLists. Will require python in path.

FG_DUMPER and lwapi codegen were not tested. Will require python in path.

Building dependencies - SDL2 and SDL_pnglite - was not tested.

Tests and fake-df build ok.


i686-apple-darwin10 build
^^^^^^^^^^^^^^^^^^^^^^^^^

Volunteers?


TTF support design
------------------

Slightly less horrible interface than the previous version.

Basically follows what the original ttf_manager did, but has all
aspects separated from each other.

``idfbuffer`` interface - ``df_buffer_t`` grew to be a backdoor replacement
for the ``class graphicsst``. Wait on actually adding it though - let it be
a part of ``isimuloop`` while it matures.

``addcoloredst()`` and ``addst()`` become wrappers around simulthread part of it,
and handle clipping by adjusting space or discarding strings altogether.
Difference - string will be discarded entirely if it starts outside the
clip rectangle. Let's see if this breaks stuff. Alternative would be to chop
off starting characters, this won't be ttf-aware at all (but what would original
code do in addst() case is even less sane). In fact, do this for addcoloredst() now.

Advance screenx by the value returned.

much uncertainity with that - does something rely on the value of screenx?
(the reason of the tab hack)?
or can I just ignore it, always return 1 or something, and do the layout myself,
with a different type of the tab hack?

stuff gets written into the buffer somehow

on buffer submission the renderer knows how to draw the text.

should keep two copies of the font open, so that glyph size lookup
and glyph render do not kill each other from different threads.

how's the data passed around?

Simuloop side when ttf's enabled:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Abbreviation is done always here.

justify_cont - I guess it's an aid for changing attrs in the middle of string.
Do as old ttf cont does and accumulate that into a single string before clipping
or otherwise processing. We'll have to do attr per char anyway.

Difference from the old code:  something surely will creep out. Abbreviation will
deviate noticeably from operating on whole string. Using space in the middle of
cont-ed string will become unsupported?

Length in pixels gets gotten from the cache, or computed and stored there.

The cache: http://timday.bitbucket.org/lru.html: abbreviated_str -> width_pixels

Difference from the ttf_manager.cpp:

Colors are not hashed because shaders don't care.

Justification is not stored because justification seems to be done only inside the
difference between grid_width*Pszx and pixel_width, so is irrelevant here.

Length in grid units gets computed wrt current Pszxy.

"Current Pszxy" is the one at the time of the buffer being added to the free_q
in the renderer. If it changes before buffer is accepted to be drawn, the frame
just gets dropped, just like with the grid size change.

Tuple of:
(abbreviated_str, grid_x, grid_y, colors, width_pixels, width_grid ~~)

Gets stored in the buffer's forward_list or something like it.
Can it be a std::forward_list? Hmm. No, it crosses module borders.

So, something pod-like, like packed forward list:

{ uint32_t record_len, grid_x, grid_y, colors, width_pixels, width_grid, ~~;
    uint8_t str[total_len - sizeof(whatever)] ~zero-padded to 8 bytes. }

in some chunk of memory independent of ptr - see df_buffer.h

The renderer side:
^^^^^^^^^^^^^^^^^^

Has an LRU cache for itself: string -> SDL_Surface of it (blend mode, although
only the alpha channel is of interest).

Plus, ideally, it would need a representation of this cache as a texture album,
so that just rect/tex coords can be streamed to the blitter.

Now, an LRU texalbum of variable-sized glyphs is an overkill, really.
Since DF is not going to be internationalized any time soon, just have the codepage
prerendered and uploaded.

DF also doesn't use styles, so it's just 256 glyphs max.

So, a std::vector of glyph rects in the prerendered texture, indexed by the codepage
codepoint, not unicode, and let the GPU sort'em out.


Resize strategy:
^^^^^^^^^^^^^^^^

Whatever. Let's make it work first.

Thus, font height is fixed at Pszy, that is grid cell height in pixels.

Any change triggers cache flushes and texalbum reupload.

Sizes below ``ttf_floor`` (like, 8px or something) disable ttf entirely.

Much room for thought, though.

For example, a ``ttf_ceil`` might make sense, since 32x32 tiles might make sense,
but 32pt font - much less likely.
