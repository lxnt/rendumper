cmake_minimum_required(VERSION 2.8)
project(rendumper)

set(CMAKE_INSTALL_PREFIX "/home/lxnt/00DFGL/dfpy")

if(UNIX)
    set(CMAKE_CXX_COMPILER g++-4.5)
    set(CMAKE_CXX_FLAGS "-m32 -march=i686 -mtune=generic -std=c++0x -Wall -Wno-sign-compare -g")
    set(CMAKE_C_FLAGS "-m32 -march=i686 -mtune=generic -Wall -Wno-sign-compare -g")
endif()

set(LWAPI_HDRS
    enums.h
    bitfields.h
    structs.h
    globals.h
)
# Generation

set_source_files_properties(${lwapi_hdrs} properties header_file_only true generated true)

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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=8)
endif()

add_definitions( -D_GNU_SOURCE=1 -D_REENTRANT )
add_subdirectory(g_src)

install(FILES renderer.py DESTINATION libs/python3.2/site-packages)
install(PROGRAMS df DESTINATION "./")

set(CPACK_GENERATOR TBZ2)
set(CPACK_PACKAGE_NAME "rendumper")
set(CPACK_PACKAGE_VENDOR "lxnt") 
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "stock libgraphics.so with fgdumper")
include(CPack)

