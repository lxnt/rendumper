
add_definitions(-Dlinux) # music_and_sound_fmodex.cpp music_and_sound_g.h
add_definitions(-Dunix)  # curses.h enabler.cpp KeybindingScreen.cpp win32_compat.cpp
add_definitions(-DCURSES) # enabler.cpp
 
option(WITH_FMOD "include fmod" OFF)
if (WITH_FMOD)
else()
    add_definitions(-DNO_FMOD=1)
endif()

set ( LIBS 
    SDL-1.2
    sndfile
    SDL_image-1.2
    SDL_ttf-2.0
    GLU
    GLEW
    m
    png12
    rt
    openal
    lwapi
)

# find_files.cpp excluded ; it refers to headers missing
set( SOURCE
    basics.cpp
    command_line.cpp
    enabler.cpp
    enabler_input.cpp
    files.cpp
    find_files_posix.cpp
    fugr_dump.cc
    graphics.cpp
    init.cpp
    interface.cpp
    keybindings.cpp
    KeybindingScreen.cpp
    music_and_sound_fmodex.cpp
    music_and_sound_openal.cpp
    random.cpp
    renderer_offscreen.cpp
    resize++.cpp
    textlines.cpp
    textures.cpp
    ttf_manager.cpp
    ViewBase.cpp
    win32_compat.cpp
)

set( HEADERS
    basics.h
    bimap.h
    command_line.h
    curses.h
    enabler.h
    enabler_input.h
    endian.h
    files.h
    find_files.h
    g_basics.h
    graphics.h
    init.h
    interface.h
    KeybindingScreen.h
    keybindings.h
    mail.hpp
    music_and_sound_fmodex.h
    music_and_sound_g.h
    music_and_sound_openal.h
    music_and_sound_v.h
    platform.h
    random.h
    renderer_2d.hpp
    renderer_curses.cpp
    renderer_opengl.hpp
    resize++.h
    svector.h
    template.h
    textlines.h
    texture_handler.h
    ttf_manager.hpp
    ViewBase.h
)

add_library( graphics SHARED ${SOURCE} )
find_package(GTK2 2.10 REQUIRED gtk)
include_directories(${GTK2_INCLUDE_DIRS})
target_link_libraries(graphics ${GTK2_LIBRARIES} ${LIBS})
install(TARGETS graphics LIBRARY DESTINATION ${LIBG_DESTINATION})

