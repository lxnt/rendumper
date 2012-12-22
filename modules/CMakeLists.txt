cmake_minimum_required(VERSION 2.8)
project(dfmodules)

# kludge
set(ENV{PKG_CONFIG_PATH} "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/")

if (NOT UNIX)
    message(FATAL_ERROR "~")
endif()

set(CMAKE_C_FLAGS "-fvisibility=hidden -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
set(CMAKE_CXX_FLAGS "-std=c++0x -fvisibility-inlines-hidden ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

set(M32 "-m32 -march=i686")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH64 TRUE)
else()
  set(ARCH64 FALSE)
endif()

if(NOT CONFIGURING_RENDUMPER)
    option(DF_32BIT_COMPAT "Compile for linking with Dwarf Fortress" ON)
else()
    set(DF_32BIT_COMPAT TRUE)
endif()

if(DF_32BIT_COMPAT)
    set(CMAKE_CXX_COMPILER g++-4.5)
    set(CMAKE_C_COMPILER g++-4.5)
endif()

if (ARCH64)
    if(DF_32BIT_COMPAT)
        set(CMAKE_LIBRARY_PATH /usr/lib32 /usr/lib/i386-linux-gnu)
        set(CMAKE_CXX_FLAGS "${M32} ${CMAKE_CXX_FLAGS}")
        set(CMAKE_C_FLAGS "${M32} ${CMAKE_C_FLAGS}")
    endif()
endif()

include(FindPkgConfig)
find_package(Threads REQUIRED)

set(DFMOD_DESTDIR lib/dfmodules)

pkg_check_modules(PKG_SDL2 QUIET sdl2)
pkg_check_modules(PKG_OPENAL QUIET openal)
pkg_check_modules(PKG_SNDFILE QUIET ncurses)
pkg_check_modules(PKG_NCURSESW QUIET ncursesw)
pkg_check_modules(PKG_GLEW QUIET glew)

if (NOT PKG_NCURSESW_FOUND)
    pkg_check_modules(PKG_NCURSES QUIET ncurses)
endif()

if (PKG_OPENAL_FOUND AND PKG_SNDFILE_FOUND)
    set(OPENAL_DEPS ON)
else()
    set(OPENAL_DEPS OFF)
endif()

set(HAVE_NCURSES OFF)
if (UNIX)
    if (PKG_NCURSES_FOUND)
        set(HAVE_NCURSES ON)
        set(NCURSES_CFLAGS "${PKG_NCURSES_CFLAGS_OTHER}")
        set(NCURSES_INCLUDE_DIRS "${PKG_NCURSES_INCLUDE_DIRS}")
        set(NCURSES_LIBRARIES "${PKG_NCURSES_LIBRARIES}")
    endif()

    if (PKG_NCURSESW_FOUND)
        set(HAVE_NCURSES ON)
        set(NCURSES_CFLAGS "${PKG_NCURSESW_CFLAGS_OTHER}")
        set(NCURSES_INCLUDE_DIRS ${PKG_NCURSESW_INCLUDE_DIRS})
        set(NCURSES_LIBRARIES ${PKG_NCURSESW_LIBRARIES})
    endif()
endif()

option(SOUND_FMOD "build sound-fmodex module" OFF)
if (SOUND_FMOD)
    add_library(sound_fmodex SHARED sound-fmodex/module.cpp)
    target_link_libraries(sound_fmodex fmodex_or_something)
    set_target_properties(sound_fmodex PROPERTIES PREFIX "")
    install(TARGETS sound_fmodex LIBRARY DESTINATION ${DFMOD_DESTDIR})
endif()

include_directories(include)
include_directories(common)
set(HEADERS
    include/ideclspec.h
    include/emafilter.h
    include/imqueue.h
    include/isimuloop.h
    include/itypes.h
    include/glue.h
    include/iplatform.h
    include/ikeyboard.h
    include/imusicsound.h
    include/itextures.h
    include/irenderer.h )
install(FILES ${HEADERS} DESTINATION include/dfmodules)

set(GL_COMMON
    common/la_muerte_el_gl.cpp
    common/df_buffer.cpp
)

SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

option(SOUND_OPENAL "build sound-openal module" OFF)
if (SOUND_OPENAL)
    if(NOT OPENAL_DEPS)
        message(SEND_ERROR "OpenAL or sndfile not found.")
    endif()
    add_library(sound_openal SHARED sound-openal/module.cpp)
    set_property(SOURCE sound-openal/module.cpp APPEND PROPERTY COMPILE_FLAGS ${PKG_OPENAL_CFLAGS})
    set_property(SOURCE sound-openal/module.cpp APPEND PROPERTY COMPILE_FLAGS ${PKG_SNDFILE_CFLAGS})
    target_link_libraries(sound_openal ${PKG_OPENAL_LIBRARIES} ${PKG_SNDFILE_LIBRARIES})
    set_target_properties(sound_openal PROPERTIES PREFIX "")
    install(TARGETS sound_openal LIBRARY DESTINATION ${DFMOD_DESTDIR})
endif()

option(PLATFORM_SDL2 "build sdl2 platform module" ${PKG_SDL2_FOUND})
if (PLATFORM_SDL2)
    if(NOT PKG_SDL2_FOUND)
        message(SEND_ERROR "SDL 2.0 not found.")
    endif()
    link_directories(${PKG_SDL2_LIBRARY_DIRS})
    add_library(platform_sdl2 SHARED platform-sdl2/platform.cpp platform-sdl2/mqueue.cpp common/df_glob.cpp)
    include_directories( ${PKG_SDL2_INCLUDE_DIRS} )
    target_link_libraries(platform_sdl2 ${PKG_SDL2_LIBRARIES})
    set_target_properties(platform_sdl2 PROPERTIES PREFIX "")
    install(TARGETS platform_sdl2 LIBRARY DESTINATION ${DFMOD_DESTDIR})
endif()

option(RENDERER_SDL2GL2 "build sdl2+gl2 renderer module" ${PKG_SDL2_FOUND})
if (RENDERER_SDL2GL2)
    if(NOT PKG_SDL2_FOUND)
        message(SEND_ERROR "SDL 2.0 not found.")
    endif()
    if(NOT PKG_GLEW_FOUND)
        message(SEND_ERROR "GLEW not found.")
    endif()
    link_directories(${PKG_SDL2_LIBRARY_DIRS})
    link_directories(${PKG_GLEW_LIBRARY_DIRS})
    include_directories( ${PKG_SDL2_INCLUDE_DIRS} )
    include_directories( ${PKG_GLEW_INCLUDE_DIRS} )
    add_library(renderer_sdl2gl2 SHARED renderer-sdl2gl2/module.cpp renderer-sdl2gl2/textures.cpp ${GL_COMMON})
    target_link_libraries(renderer_sdl2gl2 ${PKG_SDL2_LIBRARIES} ${PKG_GLEW_LIBRARIES} SDL_pnglite z)
    set_target_properties(renderer_sdl2gl2 PROPERTIES PREFIX "")
    install(TARGETS renderer_sdl2gl2 LIBRARY DESTINATION ${DFMOD_DESTDIR})
endif()

if (UNIX)
option(RENDERER_NCURSES "build ncurses renderer module" ${HAVE_NCURSES})
if (RENDERER_NCURSES)
    if(NOT HAVE_NCURSES)
        message(SEND_ERROR "ncursesw/ncurses not found.")
    endif()
    add_library(renderer_ncurses SHARED renderer-ncurses/renderer.cpp)
    include_directories( ${NCURSES_INCLUDE_DIRS} )
    target_link_libraries(renderer_ncurses ${NCURSES_LIBRARIES})
    set_target_properties(renderer_ncurses PROPERTIES PREFIX "")
    install(TARGETS renderer_ncurses LIBRARY DESTINATION ${DFMOD_DESTDIR})
endif()

option(PLATFORM_NCURSES "build ncurses platform module" ${HAVE_NCURSES})
if (PLATFORM_NCURSES)
    if(NOT HAVE_NCURSES)
        message(SEND_ERROR "ncursesw/ncurses not found.")
    endif()
    add_library(platform_ncurses SHARED platform-ncurses/platform.cpp platform-ncurses/mqueue.cpp common/df_glob.cpp)
    include_directories( ${NCURSES_INCLUDE_DIRS} )
    target_link_libraries(platform_ncurses ${NCURSES_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
    set_target_properties(platform_ncurses PROPERTIES PREFIX "")
    install(TARGETS platform_ncurses LIBRARY DESTINATION ${DFMOD_DESTDIR})
endif()
endif(UNIX)


# common code and stubs collection
set(COMMON_SRC
    common/simuloop.cpp
    common/sound-stub.cpp
    common/textures-stub.cpp
    common/df_buffer.cpp
    )
add_library(common_code SHARED ${COMMON_SRC})
set_target_properties(common_code PROPERTIES PREFIX "")
install(TARGETS common_code LIBRARY DESTINATION ${DFMOD_DESTDIR})

# the glue
add_library(glue glue.cpp)
install(TARGETS glue ARCHIVE DESTINATION ${DFMOD_DESTDIR})

# tests

add_definitions(-DDF_MODULES_PATH="${CMAKE_INSTALL_PREFIX}/${DFMOD_DESTDIR}")

add_executable(test-alert tests/alert.cpp)
target_link_libraries(test-alert glue dl ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS test-alert RUNTIME DESTINATION bin)

add_executable(test-find tests/find.cpp)
target_link_libraries(test-find glue dl ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS test-find RUNTIME DESTINATION bin)

add_executable(test-life tests/life.cpp)
target_link_libraries(test-life glue dl ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS test-life RUNTIME DESTINATION bin)
