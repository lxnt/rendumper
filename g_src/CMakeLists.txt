if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=8)
endif()

set ( LIBS m z )

if(FG_DUMPER)
    set(FGD_LIBS lwapi)
    set(FGD_SOURCE fugr_dump.cc)
endif()

set( SOURCE
    basics.cpp
    command_line.cpp
    enabler.cpp
    enabler_input.cpp
    files.cpp
    find_files.cpp
    graphics.cpp
    init.cpp
    interface.cpp
    keybindings.cpp
    KeybindingScreen.cpp
    random.cpp
    renderer.cpp
    textlines.cpp
    textures.cpp
    ViewBase.cpp
    
    platform.cpp
    music_and_sound.cpp
)

set( HEADERS
    basics.h
    bimap.h
    command_line.h
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
    music_and_sound_g.h
    platform.h
    random.h
    renderer.h
    svector.h
    stringvecst.h
    template.h
    textlines.h
    texture_handler.h
    ViewBase.h
)

if(UNIX)
    set( UNIX_ONLY_SOURCE  platform.cpp )
endif()


if(BUILD_FORTRESS)
    # pretend to build Dwarf_Fortess.exe
    add_executable(Dwarf_Fortress dwarf_fortress.cpp ${SOURCE} ${FGD_SOURCE} ${UNIX_ONLY_SOURCE})
    target_link_libraries(Dwarf_Fortress ${GLUE_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${LIBS} ${FGD_LIBS})
    install(TARGETS Dwarf_Fortress RUNTIME DESTINATION bin/libs)
endif()

if(LIBGRAPHICS)
    # build libgraphics.so
    add_library( graphics SHARED ${SOURCE} ${FGD_SOURCE} ${UNIX_ONLY_SOURCE})
    target_link_libraries(graphics glue ${LIBS} ${FGD_LIBS})
    install(TARGETS graphics LIBRARY DESTINATION lib)
endif()
