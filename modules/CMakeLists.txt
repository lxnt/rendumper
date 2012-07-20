cmake_minimum_required(VERSION 2.8)
project(dfmodules)

if(UNIX)
    set(CMAKE_CXX_COMPILER g++-4.5)
    set(CMAKE_LIBRARY_PATH /usr/lib32 /usr/lib/i386-linux-gnu)
    set(CMAKE_CXX_FLAGS_DEBUG "-m32 -g -std=c++0x -Wall -Wno-sign-compare")
    set(CMAKE_C_FLAGS_DEBUG "-m32 -g -Wall")
    set(CMAKE_CXX_FLAGS "-m32 -march=i686 -mtune=generic -std=c++0x -Wno-sign-compare ")
    set(CMAKE_C_FLAGS "-m32 -march=i686 -mtune=generic")

    set(LIB32_LINK_DIR "/home/lxnt/00DFGL/lib32" CACHE STRING "ekekek")
    link_directories(${LIB32_LINK_DIR})
endif()

file(GLOB HEADERS ${dfmodules_SOURCE_DIR}/include/*)
include_directories(${dfmodules_SOURCE_DIR}/include)
install(FILES ${HEADERS} DESTINATION include)

option(WITH_FMOD "build sound-fmodex module" OFF)
if (WITH_FMOD)
    add_library(snd_fmodex SHARED sound-fmodex/module.cpp)
    target_link_libraries(snd_fmodex fmodex_or_something)
    install(TARGETS snd_fmodex LIBRARY DESTINATION libs)
endif()

option(WITH_OPENAL "build sound-openal module" ON)
if (WITH_OPENAL)
    add_library(snd_openal SHARED sound-openal/module.cpp)
    target_link_libraries(snd_openal sndfile openal)
    install(TARGETS snd_openal LIBRARY DESTINATION libs)    
endif()

option(WITH_SOUNDSTUB "build sound-stub module" ON)
if (WITH_SOUNDSTUB)
    add_library(snd_stub SHARED sound-stub/module.cpp)
    install(TARGETS snd_stub LIBRARY DESTINATION libs)
endif()

set(PLAT_OSX_SRC 
    platform-posix/module.cpp 
    platform-posix/osx-alert.cpp )

set(PLAT_GTK_SRC
    platform-posix/module.cpp 
    platform-posix/gtk-alert.cpp )

set(PLAT_CURSES_SRC
    platform-posix/module.cpp 
    platform-posix/curses-alert.cpp )

set(PLAT_WIN32_SRC
    platform-win32/module.cpp)

if (WIN32)
    add_library(plat_win32form SHARED ${PLAT_WIN32_SRC})
    install(TARGETS plat_win32 LIBRARY DESTINATION libs)
endif()

if (UNIX)
    if (APPLE)
	add_library(plat_osx SHARED ${PLAT_OSX_SRC} )
        install(TARGETS plat_osx LIBRARY DESTINATION libs)
    endif()
    
    option(WITH_GTK "build gtk platform module" ON)
    option(WITH_OSX "build osx platform module" OFF)
    option(WITH_CURSES "build curses platform module" ON)
    if (WITH_OSX)
	add_library(plat_osx SHARED ${PLAT_OSX_SRC} )
    endif()
    if (WITH_GTK)
	add_library(plat_gtk SHARED ${PLAT_GTK_SRC} )
	find_package(GTK2 2.10 REQUIRED gtk)
	include_directories(${GTK2_INCLUDE_DIRS})
	target_link_libraries(plat_gtk ${GTK2_LIBRARIES})
        install(TARGETS plat_gtk LIBRARY DESTINATION libs)
    endif()
    if (WITH_CURSES)
	add_library(plat_curses SHARED ${PLAT_CURSES_SRC} )
        install(TARGETS plat_curses LIBRARY DESTINATION libs)
    endif()
endif()

add_library(glue glue.cpp)
install(TARGETS glue ARCHIVE DESTINATION lib)

add_executable(test test.cpp)
target_link_libraries(test glue dl)
install(TARGETS test RUNTIME DESTINATION bin)



