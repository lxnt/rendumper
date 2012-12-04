cmake_minimum_required(VERSION 2.8)
project(dfmodules)

if (NOT UNIX)
    message(FATAL_ERROR "~")
endif()

set(CMAKE_CXX_FLAGS "-ggdb3 -fvisibility=hidden -fvisibility-inlines-hidden -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3 -fvisibility=hidden -fvisibility-inlines-hidden -pthread")
set(CMAKE_C_FLAGS "-ggdb3 -fvisibility=hidden -fvisibility-inlines-hidden -pthread")
set(CMAKE_C_FLAGS_DEBUG "-ggdb3 -fvisibility=hidden -fvisibility-inlines-hidden -pthread")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH64 TRUE)
else()
  set(ARCH64 FALSE)
endif()

include(FindPkgConfig)

if (ARCH64)
    option(DF_32BIT_COMPAT "Compile for linking with Dwarf Fortress" OFF)
    if (DF_32BIT_COMPAT)
        set(CMAKE_CXX_COMPILER g++-4.5)
        set(CMAKE_LIBRARY_PATH /usr/lib32 /usr/lib/i386-linux-gnu)
        set(CMAKE_CXX_FLAGS_DEBUG "-m32 -g -std=c++0x -Wall -Wno-sign-compare")
        set(CMAKE_C_FLAGS_DEBUG "-m32 -g -Wall")
        set(CMAKE_CXX_FLAGS "-m32 -march=i686 -mtune=generic -std=c++0x -Wno-sign-compare ")
        set(CMAKE_C_FLAGS "-m32 -march=i686 -mtune=generic")
    endif()
    set(CMAKE_INSTALL_PREFIX "/home/lxnt/00DFGL/prefix32" CACHE PATH "install prefix")
else()
    set(CMAKE_INSTALL_PREFIX "/home/lxnt/00DFGL/prefix64" CACHE PATH "install prefix")
endif()

include_directories(include)
set(HEADERS
    include/ideclspec.h
    include/glue.h
    include/iplatform.h
    include/ikeyboard.h
    include/imusicsound.h
    include/itextures.h
    include/irenderer.h )
install(FILES ${HEADERS} DESTINATION include)

pkg_check_modules(PKG_SDL2 QUIET sdl2)
pkg_check_modules(PKG_OPENAL QUIET openal)
pkg_check_modules(PKG_SNDFILE QUIET ncurses)
pkg_check_modules(PKG_NCURSESW QUIET ncursesw)
if (NOT PKG_NCURSESW_FOUND)
    pkg_check_modules(PKG_NCURSES QUIET ncurses)
endif()

if (PKG_OPENAL_FOUND AND PKG_SNDFILE_FOUND)
    set(OPENAL_DEPS ON)
else()
    set(OPENAL_DEPS OFF)
endif()

set(NCURSES_DEPS OFF)
if (PKG_NCURSES_FOUND)
    set(NCURSES_DEPS ON)
    set(NCURSES_CFLAGS "${PKG_NCURSES_CFLAGS_OTHER}")
    set(NCURSES_INCLUDE_DIRS "${PKG_NCURSES_INCLUDE_DIRS}")
    set(NCURSES_LIBRARIES "${PKG_NCURSES_LIBRARIES}")
endif()
if (PKG_NCURSESW_FOUND)
    set(NCURSES_DEPS ON)
    set(NCURSES_CFLAGS "${PKG_NCURSESW_CFLAGS_OTHER}")
    set(NCURSES_INCLUDE_DIRS ${PKG_NCURSESW_INCLUDE_DIRS})
    set(NCURSES_LIBRARIES ${PKG_NCURSESW_LIBRARIES})
endif()

option(SOUND_FMOD "build sound-fmodex module" OFF)
if (SOUND_FMOD)
    add_library(sound_fmodex SHARED sound-fmodex/module.cpp)
    target_link_libraries(sound_fmodex fmodex_or_something)
    install(TARGETS sound_fmodex LIBRARY DESTINATION libs)
endif()

option(SOUND_OPENAL "build sound-openal module" ${OPENAL_DEPS})
if (SOUND_OPENAL)
    if(NOT OPENAL_DEPS)
        message(SEND_ERROR "OpenAL or sndfile not found.")
    endif()
    add_library(sound_openal SHARED sound-openal/module.cpp)
    set_property(SOURCE sound-openal/module.cpp APPEND PROPERTY COMPILE_FLAGS ${PKG_OPENAL_CFLAGS})
    set_property(SOURCE sound-openal/module.cpp APPEND PROPERTY COMPILE_FLAGS ${PKG_SNDFILE_CFLAGS})
    target_link_libraries(sound_openal ${PKG_OPENAL_LIBRARIES} ${PKG_SNDFILE_LIBRARIES})
    install(TARGETS sound_openal LIBRARY DESTINATION libs)
endif()

option(SOUND_STUB "build sound-stub module" ON)
if (SOUND_STUB)
    add_library(sound_stub SHARED sound-stub/module.cpp)
    install(TARGETS sound_stub LIBRARY DESTINATION libs)
endif()

option(PLATFORM_SDL20 "build sdl20 platform module" ${PKG_SDL2_FOUND})
if (PLATFORM_SDL20)
    if(NOT PKG_SDL2_FOUND)
        message(SEND_ERROR "SDL 2.0 not found.")
    endif()
    add_library(platform_sdl20 SHARED platform-sdl20/platform.cpp platform-sdl20/mqueue.cpp)
    include_directories( ${PKG_SDL2_INCLUDE_DIRS} )
    target_link_libraries(platform_sdl20 ${PKG_SDL2_LIBRARIES})

    install(TARGETS platform_sdl20 LIBRARY DESTINATION libs)
endif()

option(RENDERER_NCURSES "build ncurses renderer module" OFF)
if (RENDERER_NCURSES)
    if(NOT NCURSES_DEPS)
        message(SEND_ERROR "ncursesw/ncurses not found.")
    endif()
    add_library(renderer_ncurses SHARED renderer-ncurses/renderer.cpp renderer-ncurses/simuloop.cpp)
    include_directories( ${NCURSES_INCLUDE_DIRS} )
    target_link_libraries(renderer_ncurses ${NCURSES_LIBRARIES})

    install(TARGETS renderer_ncurses LIBRARY DESTINATION libs)
endif()

option(PLATFORM_NCURSES "build ncurses platform module" OFF)
if (PLATFORM_NCURSES)
    if(NOT NCURSES_DEPS)
        message(SEND_ERROR "ncursesw/ncurses not found.")
    endif()
    add_library(platform_ncurses SHARED platform-ncurses/platform.cpp platform-ncurses/mqueue.cpp)
    include_directories( ${NCURSES_INCLUDE_DIRS} )
    target_link_libraries(platform_ncurses ${NCURSES_LIBRARIES})

    install(TARGETS platform_ncurses LIBRARY DESTINATION libs)
endif()

add_library(glue glue.cpp)
install(TARGETS glue ARCHIVE DESTINATION lib)

add_executable(test test.cpp)
target_link_libraries(test glue dl)
install(TARGETS test RUNTIME DESTINATION bin)



