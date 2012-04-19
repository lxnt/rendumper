cmake_minimum_required(VERSION 2.8)
project(rendumper)

set(CMAKE_BUILD_TYPE "Debug")
set(LIBG_DESTINATION "/home/lxnt/00DFGL/df34/libs")
if(UNIX)
    set(CMAKE_CXX_COMPILER g++-4.5)
    set(CMAKE_LIBRARY_PATH /usr/lib32 /usr/lib/i386-linux-gnu)
    set(CMAKE_CXX_FLAGS_DEBUG "-m32 -g -std=c++0x -Wall -Wno-sign-compare")
    set(CMAKE_C_FLAGS_DEBUG "-m32 -g -Wall")
    set(CMAKE_CXX_FLAGS "-m32 -march=i686 -mtune=generic -std=c++0x -Wno-sign-compare ")
    set(CMAKE_C_FLAGS "-m32 -march=i686 -mtune=generic")
endif()

#include_directories(dfapi/include)
link_directories(/usr/lib/i386-linux-gnu /usr/lib32)

#if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
#    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING 
#        "Choose the type of build, options are: Debug Release."
#    )
#endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=8)
endif()

add_definitions( -D_GNU_SOURCE=1 -D_REENTRANT )

add_subdirectory(g_src)
#add_subdirectory(dfapi)

set(CPACK_GENERATOR TBZ2)
set(CPACK_PACKAGE_NAME "rendumper")
set(CPACK_PACKAGE_VENDOR "lxnt") 
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "stock libgraphics.so with fgdumper")
include(CPack)

