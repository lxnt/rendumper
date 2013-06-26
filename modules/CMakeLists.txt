cmake_minimum_required(VERSION 2.8)
project(dfmodules)

if (UNIX)
    set(DFMOD_DESTDIR "lib/dfmodules")
else()
    set(DFMOD_DESTDIR "bin/libs")
endif()

find_package(Threads REQUIRED)

if(MSVC)
    # nothing
else(MSVC)
    # kludge. the horror.
    set(ENV{PKG_CONFIG_PATH} "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/")

    set(CMAKE_C_FLAGS "-fvisibility=hidden -Wall -Wextra -Werror")
    set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
    set(CMAKE_CXX_FLAGS "-std=c++0x -fvisibility-inlines-hidden ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

    # zhban:
    include_directories(${CMAKE_INSTALL_PREFIX}/include)

    if (UNIX)
        if(NOT BUILD_LIBGRAPHICS AND ARCH64)
            option(BUILD_32BIT "Compile 32bit binaries" ON)
        else()
            set(BUILD_32BIT TRUE)
        endif()

        if(BUILD_32BIT)
            if (ARCH64)
                set(CMAKE_LIBRARY_PATH /usr/lib32 /usr/lib/i386-linux-gnu)
                set(CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
                set(CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
            endif()
        endif()

        set(CMAKE_SKIP_BUILD_RPATH  FALSE)
        set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
        set(CMAKE_INSTALL_RPATH "\$ORIGIN")
        set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

    endif()

    include(FindPkgConfig)

    pkg_check_modules(PKG_SDL2 QUIET sdl2)
    #pkg_check_modules(PKG_OPENAL QUIET openal)
    #pkg_check_modules(PKG_SNDFILE QUIET sndfile)
    pkg_check_modules(PKG_GLEW QUIET glew)
    pkg_check_modules(PKG_SDL2MIXER QUIET SDL2_mixer)

endif(MSVC)

if (PKG_OPENAL_FOUND AND PKG_SNDFILE_FOUND)
    set(OPENAL_DEPS ON)
else()
    set(OPENAL_DEPS OFF)
endif()

set(HAVE_NCURSES OFF)
if (UNIX)
    pkg_check_modules(PKG_NCURSESW QUIET ncursesw)

    if (NOT PKG_NCURSESW_FOUND)
        pkg_check_modules(PKG_NCURSES QUIET ncurses)
    endif()

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

if(FMOD_BUILD_ENABLED)
    option(SOUND_FMOD "build sound-fmodex module" OFF)
    if (SOUND_FMOD)
        add_library(sound_fmodex SHARED sound-fmodex/module.cpp)
        target_link_libraries(sound_fmodex fmodex_or_something)
        set_target_properties(sound_fmodex PROPERTIES PREFIX "")
        install(TARGETS sound_fmodex LIBRARY DESTINATION ${DFMOD_DESTDIR})
    endif()
endif()


option(LOGGING_DEBUG "Trace logging work" OFF)
if (LOGGING_DEBUG)
   set_source_files_properties( common/logging.cpp PROPERTIES COMPILE_DEFINITIONS LOGGING_LOGGING )
endif()

include_directories(common)
set(PLATFORM_COMMON
    common/df_glob.cpp
    common/df_buffer.cpp
    common/logging.cpp
    common/settings.cpp
)

set_source_files_properties(shaders.c PROPERTIES GENERATED TRUE)
file(GLOB SHADER_SOURCE shaders/*)
add_custom_command(
    OUTPUT ${dfmodules_SOURCE_DIR}/common/shaders.cpp
    COMMAND python ${dfmodules_SOURCE_DIR}/grab-shaders.py ${dfmodules_SOURCE_DIR}/shaders ${dfmodules_SOURCE_DIR}/common/shaders.cpp
    WORKING_DIRECTORY ${dfmodules_SOURCE_DIR}
    MAIN_DEPENDENCY grab-shaders.py
    DEPENDS ${SHADER_SOURCE}
)
add_custom_target(grab_shaders DEPENDS common/shaders.cpp)

set(GL_COMMON
    common/la_muerte_el_gl.cpp
    common/df_buffer.cpp
    common/shaders.cpp
)

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
#    install(TARGETS sound_openal LIBRARY DESTINATION ${DFMOD_DESTDIR})
    install(TARGETS sound_openal LIBRARY DESTINATION ${DFMOD_DESTDIR} RUNTIME DESTINATION ${DFMOD_DESTDIR} ARCHIVE DESTINATION ${DFMOD_DESTDIR})

endif()

option(SOUND_SDL2MIXER "build sound-sdl2mixer module" OFF)
if (SOUND_SDL2MIXER)
    if(NOT PKG_SDL2_FOUND)
        message(SEND_ERROR "SDL 2.0 not found.")
    endif()
    if(NOT PKG_SDL2_MIXER_FOUND)
        message(SEND_ERROR "SDL_Mixer 2.0 not found.")
    endif()
    link_directories(${PKG_SDL2_LIBRARY_DIRS})
    link_directories(${PKG_SDL2MIXER_LIBRARY_DIRS})
    include_directories( ${PKG_SDL2_INCLUDE_DIRS} )
    include_directories( ${PKG_SDL2MIXER_INCLUDE_DIRS} )
    add_library(sound_sdl2mixer SHARED sound-sdl2mixer/module.cpp)
    target_link_libraries(sound_sdl2mixer ${PKG_SDL2MIXER_LIBRARIES} ${PKG_SDL2_LIBRARIES})
    set_target_properties(sound_sdl2mixer PROPERTIES PREFIX "")
    install(TARGETS sound_sdl2mixer LIBRARY DESTINATION ${DFMOD_DESTDIR} RUNTIME DESTINATION ${DFMOD_DESTDIR} ARCHIVE DESTINATION ${DFMOD_DESTDIR})
endif()

option(PLATFORM_SDL2 "build sdl2 platform module" ${PKG_SDL2_FOUND})
if (PLATFORM_SDL2)
    if(NOT PKG_SDL2_FOUND)
        message(SEND_ERROR "SDL 2.0 not found.")
    endif()
    link_directories(${PKG_SDL2_LIBRARY_DIRS})
    add_library(platform_sdl2 SHARED platform-sdl2/platform.cpp platform-sdl2/mqueue.cpp ${PLATFORM_COMMON})
    include_directories( ${PKG_SDL2_INCLUDE_DIRS} )
    target_link_libraries(platform_sdl2 ${PKG_SDL2_LIBRARIES})
    set_target_properties(platform_sdl2 PROPERTIES PREFIX "")
    install(TARGETS platform_sdl2 LIBRARY DESTINATION ${DFMOD_DESTDIR} RUNTIME DESTINATION ${DFMOD_DESTDIR} ARCHIVE DESTINATION ${DFMOD_DESTDIR})
endif()

option(RENDERER_SDL2GL2 "build sdl2+gl2 renderer module" ${PKG_SDL2_FOUND})
if (RENDERER_SDL2GL2)
    if(NOT PKG_SDL2_FOUND)
        message(SEND_ERROR "SDL 2.0 not found.")
    endif()
    if (UNIX)
        if(NOT PKG_GLEW_FOUND)
            message(SEND_ERROR "GLEW not found.")
        endif()
        link_directories(${PKG_GLEW_LIBRARY_DIRS})
        include_directories( ${PKG_GLEW_INCLUDE_DIRS} )
    else()
        include_directories("${CMAKE_INSTALL_PREFIX}/include/GL")
    endif()
    link_directories(${PKG_SDL2_LIBRARY_DIRS})
    include_directories( ${PKG_SDL2_INCLUDE_DIRS} )
    add_library(renderer_sdl2gl2 SHARED renderer-sdl2gl2/module.cpp renderer-sdl2gl2/textures.cpp ${GL_COMMON})
    set_target_properties(renderer_sdl2gl2 PROPERTIES PREFIX "")
    set_target_properties(renderer_sdl2gl2 PROPERTIES COMPILE_DEFINITIONS DFM_SHADERS_PATH="data/shaders")
    if (UNIX)
        target_link_libraries(renderer_sdl2gl2 ${PKG_SDL2_LIBRARIES} ${PKG_GLEW_LIBRARIES} SDL_pnglite z)
        install(TARGETS renderer_sdl2gl2 LIBRARY DESTINATION ${DFMOD_DESTDIR})
    else()
        target_link_libraries(renderer_sdl2gl2 SDL_pnglite z ${PKG_SDL2_LIBRARIES} glew32 opengl32 glu32)
        install(TARGETS renderer_sdl2gl2 LIBRARY DESTINATION ${DFMOD_DESTDIR} RUNTIME DESTINATION ${DFMOD_DESTDIR} ARCHIVE DESTINATION ${DFMOD_DESTDIR})
    endif()
endif()

option(RENDERER_SDL2GL3 "build sdl2+gl3renderer module" ${PKG_SDL2_FOUND})
if (RENDERER_SDL2GL3)
    if(NOT PKG_SDL2_FOUND)
        message(SEND_ERROR "SDL 2.0 not found.")
    endif()
    if (UNIX)
        if(NOT PKG_GLEW_FOUND)
            message(SEND_ERROR "GLEW not found.")
        endif()
        link_directories(${PKG_GLEW_LIBRARY_DIRS})
        include_directories( ${PKG_GLEW_INCLUDE_DIRS} )
    else()
        include_directories("${CMAKE_INSTALL_PREFIX}/include/GL")
    endif()
    link_directories(${PKG_SDL2_LIBRARY_DIRS})
    include_directories( ${PKG_SDL2_INCLUDE_DIRS} )
    add_library(renderer_sdl2gl3 SHARED renderer-sdl2gl3/module.cpp renderer-sdl2gl2/textures.cpp ${GL_COMMON})
    set_target_properties(renderer_sdl2gl3 PROPERTIES PREFIX "")
    set_target_properties(renderer_sdl2gl3 PROPERTIES COMPILE_DEFINITIONS DFM_SHADERS_PATH="data/shaders")
    if (UNIX)
        target_link_libraries(renderer_sdl2gl3 ${PKG_SDL2_LIBRARIES} ${PKG_GLEW_LIBRARIES} zhban SDL_pnglite z)
        install(TARGETS renderer_sdl2gl3 LIBRARY DESTINATION ${DFMOD_DESTDIR})
    else()
        target_link_libraries(renderer_sdl2gl3 SDL_pnglite z ${PKG_SDL2_LIBRARIES} zhban glew32 opengl32 glu32)
        install(TARGETS renderer_sdl2gl3 LIBRARY DESTINATION ${DFMOD_DESTDIR} RUNTIME DESTINATION ${DFMOD_DESTDIR} ARCHIVE DESTINATION ${DFMOD_DESTDIR})
    endif()
endif()

if (UNIX)
option(RENDERER_NCURSES "build ncurses renderer module" ${HAVE_NCURSES})
    if (RENDERER_NCURSES)
        if(NOT HAVE_NCURSES)
            message(SEND_ERROR "ncursesw/ncurses not found.")
        endif()
        add_library(renderer_ncurses SHARED renderer-ncurses/renderer.cpp common/cp437.cpp common/df_buffer.cpp)
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
        add_library(platform_ncurses SHARED platform-ncurses/platform.cpp platform-ncurses/mqueue.cpp ${PLATFORM_COMMON})
        include_directories( ${NCURSES_INCLUDE_DIRS} )
        target_link_libraries(platform_ncurses ${NCURSES_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
        set_target_properties(platform_ncurses PROPERTIES PREFIX "")
        install(TARGETS platform_ncurses LIBRARY DESTINATION ${DFMOD_DESTDIR})
    endif()
endif(UNIX)


# common code and stubs collection
set(COMMON_SRC
    common/common_code_deps.cpp
    common/cp437.cpp
    common/simuloop.cpp
    common/sound-stub.cpp
    common/textures-stub.cpp
    common/df_buffer.cpp
    )
add_library(common_code SHARED ${COMMON_SRC})
set_target_properties(common_code PROPERTIES PREFIX "")
target_link_libraries(common_code m)
if (UNIX)
    install(TARGETS common_code LIBRARY DESTINATION ${DFMOD_DESTDIR})
else()
    install(TARGETS common_code RUNTIME DESTINATION ${DFMOD_DESTDIR})
endif()

