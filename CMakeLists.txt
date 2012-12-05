cmake_minimum_required(VERSION 2.8)
project(rendumper)

set(LIBG_DESTINATION "/home/lxnt/00DFGL/df34/libs" CACHE STRING "kekeke")
if(UNIX)
    set(CMAKE_CXX_COMPILER g++-4.5)
    set(CMAKE_LIBRARY_PATH /usr/lib32 /usr/lib/i386-linux-gnu)
    set(CMAKE_CXX_FLAGS_DEBUG "-m32 -g -std=c++0x -Wall -Wno-sign-compare")
    set(CMAKE_C_FLAGS_DEBUG "-m32 -g -Wall")
    set(CMAKE_CXX_FLAGS "-m32 -march=i686 -mtune=generic -std=c++0x -Wno-sign-compare ")
    set(CMAKE_C_FLAGS "-m32 -march=i686 -mtune=generic")
endif()

option(RENDER_CURSES "Compile curses renderer" ON)
option(RENDER_2D     "Compile curses renderer" OFF)
option(RENDER_GL     "Compile curses renderer" OFF)

if(RENDER_CURSES)
    add_definitions(-DRENDER_CURSES)
endif()
if(RENDER_2D)
    add_definitions(-DRENDER_2D -DRENDER_SDL)
endif()
if(RENDER_GL)
    add_definitions(-DRENDER_GL -DRENDER_SDL)
endif()

set(LWAPI_HDRS
    enums.h
    bitfields.h
    structs.h
    globals.h
)
# Generation
set_source_files_properties(${lwapi_hdrs} properties header_file_only true generated true)

file(GLOB generate_input_xmls ${rendumper_source_dir}/df-structures/*.xml)

set(lwapi_dir ${rendumper_SOURCE_DIR}/lwapi)
set(dfstr_dir ${rendumper_SOURCE_DIR}/df-structures)

file(GLOB api_xml ${dfstr_dir}/*.xml)
add_custom_command(
    OUTPUT ${lwapi_dir}/globals.cc
    COMMAND ${dfstr_dir}/codegen.py -dst ${lwapi_dir}
    WORKING_DIRECTORY ${dfstr_dir}
    MAIN_DEPENDENCY ${dfstr_dir}/codegen.py
    DEPENDS ${dfstr_dir}/static.py ${api_xml}
)

add_custom_target(generate_code)
add_library(lwapi ${lwapi_dir}/globals.cc )
add_dependencies(lwapi generate_code)

include_directories(${rendumper_SOURCE_DIR}/lwapi)
#link_directories(${rendumper_SOURCE_DIR}/lwapi)
link_directories(/usr/lib/i386-linux-gnu /usr/lib32)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=8)
endif()

add_definitions( -D_GNU_SOURCE=1 -D_REENTRANT )
add_subdirectory(g_src)

set(CPACK_GENERATOR TBZ2)
set(CPACK_PACKAGE_NAME "rendumper")
set(CPACK_PACKAGE_VENDOR "lxnt") 
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "stock libgraphics.so with fgdumper")
include(CPack)

