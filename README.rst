This is DF modular backend
**************************

You can go straight to `Binary releases`_

Project goal
------------

Fully customizable graphics for the Dwarf Fortress.

To that end, first split out all platform-dependent code from the main executable into plugins.

This leaves a mostly clean interface between the game and the graphics code.

Improve it a bit, and the game doesn't know or care about graphics, while at the same time
plugins allow modding the hfs out of all of them.


Todo list
---------

Issues:
^^^^^^^

- Movie playback (intro, etc) is broken in non-80x25 window size.

Under construction:
^^^^^^^^^^^^^^^^^^^

- OpenAL sound module : in progress
- SoundSense soundpack support in the sound module.
  Depends on MP3 (and other) streaming decoders. XML parse offloaded to
  a preprocessor.

Next:
^^^^^

- third-party memory-hacking tool support - do the fgdumper for an example.

In no particular order:
^^^^^^^^^^^^^^^^^^^^^^^

- have an internal locked buffer for the log, flush it once in a while, ala ucoev.
- clean up TTF-related interfaces
- decide if data races are really wanted. move everything to mqueues if not.
- clean up itc_message_t - half done
- compile with the same flags as SDL is: -mmx -3dnow -sse
- make a knob for max-optimization builds.
- show fps+averaged times on an overlay. maybe do a graph ala eve online
- rewrite df-structures/codegen.py, it rotted; revive : it now works, but in
  dire need of ground-up rewrite. 8x code size and ~133x file count reduction as
  compared to the the stock codegen are still desirable.
- sdl2gl* renderers - move common code to common/
- merge ui and compositor from fgtestbed. first finish and debug it though
- sdl2gl2 GL_POINT positioning suffers rounding errors: eats pixels.
  visible with DF_LOG=sdl.reshape=trace.
- Do something like replacing std::forward_list with utlist.h so that it doesn't spam .so/.dll namespace.
  http://gcc.gnu.org/bugzilla/show_bug.cgi?id=36022
  Basically, libstdc++ stuff can't be hidden. This isn't fatal for gcc-built builds,
  but still looks dirty.
  Solution: get rid of as much as possible from libstdc++.
- Actually release resources at exit
- Decide what to do with 2D world map drawing and export (Legends mode).
  Currently it's SDL_SaveBMP buried in the binary, and while it happens to work with
  SDL2.0 binary substituted for SDL1.2, it's fragile and just plain wrong.
- offload  dim/rain/snow effects to the renderer, implement in shaders,
  reimplement original in the ncurses renderer. = does not work because
  relevant graphicst methods are not getting called.
- maybe get rid of vbstreamer in sdl2gl2.


Stuff I put a SEP field around:
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- pure SDL2 renderer ala PRINT_MODE:2D
- Offscreen ncurses renderer complete with finding the appropriate file
  format and viewer
- i686-apple-darwin10 build
- ncurses mouse input

Older notes
^^^^^^^^^^^

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

What's on inside
----------------

Modules and interfaces
^^^^^^^^^^^^^^^^^^^^^^

Interfaces are what I ended up when I split whatever functionality Dwarf
Fortress needs into more-or-less complete blocks, whose dependencies could
then be simple. These are:

- iplatform - low-level initialization, filesystem interface (globs), logging, threads
- imqueue - interthread message queue
- itextures - texture manager, loads and packs tile/creature graphics into albums
- isimuloop - implements the simulation thread, passing around commands and buffers,
             and controlling both simulation and rendering frame rates.
- irenderer - implements the renderer thread, which renders buffers, hands over buffers
             to the simulation thread, converts and passes on user input.
- imusicsound - intended to host the audio code, work very in progress.

In the simplest case, the game consists of two threads - renderer and simulation,
where renderer thread is usually the main one. When renderer module starts up, it
initializes itself, whatever video output it likes to, and starts supplying
df_buffer_t structures to the simuloop. Any user input is also supplied there,
both going via a message queue. Message types are a part of overall interface,
see include/itypes.h.

The simuloop thread accepts those buffers and input, passing them via callbacks to the
actual game code. Input is fed as it arrives, while calls to 'mainloop' - simulation callback,
and 'render_things' - rendering callback are delayed so as to not exceed set framerate limits.

The 'assimilate_buffer' callback is expected to set up the game code so that the following
call to 'render_things' will fill it up with scene for the next frame. After that, another
call to 'assimilate_buffer', with NULL for the buffer, detaches the buffer from the game code,
and it is given back to the renderer.

The code that implements the above is split into four types of shared objects/dynamic libraries/plugins.
Those are:

- plaform_P - contains iplatform and imqueue implementations for a given platform. "P" in the name stands
  for the platform name. There are two plaforms currently supported - ncurses and SDL2, the latter on both
  windows and linux (and wherever else SDL2 more-or-less works).
- common_code - contains isimuloop implementation, which is currently platform-independent in the sense
  that it doesn't depend on any particular iplatform implementation. Also contains stub implementations of
  imusicsound and itextures for completeness when there are no platform-specific versions available.
- renderer_PT - contains irenderer and itextures implementations. It depends on platform_P being available,
  and is further distinguished from other renderer for the platform by suffix "T".
- sound_PT or sound_T - is intended to contain imusicsound implementation, currently there is none.

The game executable is linked with a static library libglue, which contains plugin loader and linker,
and works on both windows and linux.

After successful load of the plugin set, configuration data can be fed via iplatform's set_setting(),
then simuloop is set up with callbacks, threads are started and game goes on.

See modtests/life.cpp for a trivial example, or g_src/enabler.cpp for how it is done for the Dwarf Fortress itself.

How it plugs into Dwarf Fortress
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Answer: easily.

As you might know, the source code in g_src, is a part of the game.
On windows it is compiled in into the main executable, on linux - into a separate library, libs/libgraphics.so.

Code in the g_src directory in this project is a heavily patched version of it, where everything non-generic
was replaced with calls to the interfaces described above, and the plugin loader was added.

Thus, once the game gets recompiled for windows with the g_src code from here, it will rely on plugins for all
the rendering, sound, etc.

Logging
^^^^^^^

Stub documentation:

grep the source for 'getlogr' to know what loggers are there.
Set loglevels like this::

    DF_LOG=sdl.input=trace,sdl.textures=info ./df

platform_ncurses writes logs into 'dfm.log' file.

Binary releases
---------------

Binary releases of this code for linux can be downloaded from http://sourceforge.net/projects/tolisnitem/

Archive name format is YearMonthDay-Hour.7z, in UTC+0 timezone. They are uploaded not very regularly, so
please consult git commit log at https://github.com/lxnt/rendumper/commits/interfaces if it crashes or
misbehaves - this particular bug might have been fixed already.

To install, make a copy of Dwarf Fortress directory, then unzip the archive into the Dwarf Fortress directory.

You should end up with something like::

    libs/common_code.so
    libs/Dwarf_Fortress
    libs/libGLEW.so.1.6
    libs/libgraphics.so
    libs/libharfbuzz.so.0
    libs/libSDL-1.2.so.0
    libs/libSDL2-2.0.so.0
    libs/libSDL2_mixer-2.0.so.0
    libs/libzhban.so
    libs/platform_ncurses.so
    libs/platform_sdl2.so
    libs/renderer_ncurses.so
    libs/renderer_sdl2gl2.so
    libs/renderer_sdl2gl3.so
    libs/sound_sdl2mixer.so

You will need the following libraries installed system-wide for the sound to work (those are Ubuntu names)::

    libvorbisfile3:i386
    libflac8:i386

If you have latest open-source OpenGL drivers installed (mesa), delete the following files::

    libs/libgcc_1.so
    libs/libstdc++.so.6

as they will prevent the game from starting.

If you have 64bit system, make sure you have appropriate i386 mesa packages installed.
Basically, if vanilla Dwarf Fortress in some OpenGL PRINT_MODE runs ok, you're set.

Having done that, launch `df` as usual, this will load SDL2 OpenGL 3.0 renderer with TTF support.

Other renderers avaliable are ncurses and sdl2gl2, they are selected by giving an argument::

    ./df ncurses
    ./df sdl2gl2

Note however, that main development goes in sdl2gl3, and those two may lag behind in bugfixes.

TTF support is activated if and only if graphics tileset tile height equals the [TRUETYPE] setting
in data/init/init.txt. For example if you've got some 16x16 tileset installed, put [TRUETYPE:16] there.

For the ease of testing, F12 key is hardcoded to nastily abort the program.

Building this:
--------------

i686-linux-gnu build
^^^^^^^^^^^^^^^^^^^^

Due to C++ ABI hell and autotools' excessive arcanism, the recommended build
method is the native one.

Consider using a virtual machine (KVM or whatever) with a minimal 32-bit Ubuntu 12.04 install.

Make sure you have GCC 4.5 installed, and /usr/bin/gcc and /usr/bin/g++ symlinks pointing to it.

Install the following packages:

- git
- realpath
- libglew1.6-dev
- libfreetype6-dev,
- zlib1g-dev
- uthash-dev (1.9.8)
- libgl1-mesa-dev
- cmake-curses-gui
- wget

I might have forgotten some.

The sound module additinally requires

- libflac-dev
- libvorbis-dev
- libmodplug-dev

The init-prefix script haven't been updated for the sound module yet.
Latest smpeg (http://www.libsdl.org/projects/smpeg/release/smpeg2-2.0.0.tar.gz) and SDL2_mixer
(http://www.libsdl.org/tmp/SDL_mixer/release/SDL2_mixer-2.0.0.tar.gz) have to be installed into
the prefix.

Pull the source::

    git clone git://github.com/lxnt/rendumper.git

To fetch and build source dependencies, use the init-prefix.sh script::

    ./rendumper/init-prefix.sh deps/ build/ prefix/

This will download and/or pull needed source code into deps directory,
build them under the build directory and install into the prefix directory.

An attempt to build the modular backend itself will also be made.

After that symlink or copy the libgraphics library and the modules into the Dwarf Fortress
libs directory so that it looks like::


    lrwxrwxrwx 1 lxnt lxnt       37 Dec 31 16:38 common_code.so -> /tmp/prefix/lib/dfmodules/common_code.so
    -rwxr-xr-x 1 lxnt lxnt 15104448 Jun  4  2012 Dwarf_Fortress
    -rw-r--r-- 1 lxnt lxnt   466491 Jun  4  2012 libgcc_s.so.1.orig
    lrwxrwxrwx 1 lxnt lxnt       27 Dec 31 16:38 libgraphics.so -> /tmp/prefix/lib/libgraphics.so
    -rwxr-xr-x 1 lxnt lxnt  1451966 Jun  4  2012 libgraphics.so.orig
    lrwxrwxrwx 1 lxnt lxnt       29 Dec 31 16:39 libSDL-1.2.so.0 -> /tmp/prefix/lib/libSDL2-2.0.so.0
    -rwxr-xr-x 1 lxnt lxnt  4852343 Jun  4  2012 libstdc++.so.6.orig
    lrwxrwxrwx 1 lxnt lxnt       39 Dec 31 16:38 platform_sdl2.so -> /tmp/prefix/lib/dfmodules/platform_sdl2.so
    lrwxrwxrwx 1 lxnt lxnt       42 Dec 31 16:38 renderer_sdl2gl3.so -> /tmp/prefix/lib/dfmodules/renderer_sdl2gl3.so

Notice renamed libgcc_s.so.1 and libstdc++.so.6. You may as well delete them.

Launch as usual.

Shaders' source gets embedded into the renderer binaries, but they will attempt to read it from data/shaders directory
before using embedded one.


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

Based on the lockless caching text shaper/renderer - see https://github.com/lxnt/zhban , which was written
specifically for this project, to replace SDL_ttf giving better shaping support and cutting down
on surface/texture format conversions - the final blit is expected to use GLSL, and cached bitmaps do not
depend on attributes (colors of glyphs and background).

``graphicst::addst()`` became a simpler wrapper around simuloop::add_string().

String mutilation code is in modules/common/shrink.h

The tab hack - replacing ':\\x20 ...' with ':\\t ...' in justify_cont mode - is implemented in the libzhban,
and the addst() just replaces and concatenates.

The zhban aligns next glyph after a tab to a predefined multiple of em-width while shaping/sizing.

In all cases, there are two values returned from the renderer when sizing a sting - number of grid cells
this string will touch, assuming it starts on a grid cell boundary, and number of pixels true string width
is less than aforementioned span of grid cells. This number of pixels is used for justify_center and justify_right,
so beware that result of combining them with tab-hack is undefined - tabs will break in some way.

All strings that are to be rendered in TTF font get sized/shrinked accordingly and put into df_text_t
structure which gets attached to the currently assimilated buffer.

On buffer submission the renderer uses the other half of the zhban to draw the text at given grid coords
with given pixel offset off the left grid cell boundary - kind of left margin.

Audio
-----

sound-sdl2mixer
^^^^^^^^^^^^^^^

A bare-bones sound support, known for playing back sounds more-or-less correctly in the intro movie,
and then playing background music. Requires libSDL2_mixer. Plays whatever format SDL2_mixer supports, which
is enough for stock sounds. Basically, an example implentation of imusicsound interface.


sound-openal
^^^^^^^^^^^^

Since SDL2_mixer isn't all that advanced, the original sound-openal module is being rewritten to be properly
multithreaded - that is separating sound event scheduling, sound playback and sound file load and decoding or
streaming into distinct threads, while utilizing OpenAL-soft for mixing and positioning stuff.

Support for SoundSense soundpacks has been already written and only lacks streaming decoder and mixer support.

Threading:

- public interface is just stubs sending mqueue messages to the manager thread
- manager thread does the lightweight work - scheduling, matching SoundSense patterns, requesting
  stream-decode, etc.
- loader thread has a task queue of files to decode. (?)
- some (hidden?) mixing thread does the actual heavy lifting. This is hopefully hidden from us by the OpenAL.




