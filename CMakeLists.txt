cmake_minimum_required(VERSION 2.8)
project(rendumper)

find_package(Threads REQUIRED)
if(CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "build type" FORCE)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH64 TRUE)
else()
  set(ARCH64 FALSE)
endif()

set(GLUE_LIBS glue)

if(MSVC)
    # nothing.
else(MSVC)
    # ye olde gcc
    set(YE_OLDE_GCC TRUE)
    set(BRING_EM_ON "-Wall -Wextra -pedantic")
    set(CMAKE_C_FLAGS "${BRING_EM_ON}")
    set(CMAKE_CXX_FLAGS "-std=c++0x ${BRING_EM_ON}")
    set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
    set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "-mmmx -m3dnow -msse -O3 -ggdb3")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-mmmx -m3dnow -msse -O3 -ggdb3")
    set(CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELWITHDEBINFO})
    set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})


    if(UNIX)
        if(ARCH64)
            set(CMAKE_LIBRARY_PATH /usr/lib32 /usr/lib/i386-linux-gnu)
            set(CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
            set(CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
        endif(ARCH64)
        set(GLUE_LIBS glue dl)
    else(UNIX)
        # i686-w64-mingw32 hopefully.

    endif(UNIX)
endif(MSVC)

include_directories(glue)
include_directories(include)
add_subdirectory(glue)

option(BUILD_FORTRESS "Pretend to build Dwarf Fortress")
if(UNIX)
    option(BUILD_LIBGRAPHICS "Build libgraphics.so")
endif()

if(BUILD_LIBGRAPHICS OR BUILD_FORTRESS)
    if(NOT UNIX AND NOT MSVC)
        # add install-prefix include dir for mingw builds : zlib
        # todo: move compression into the platform so/dll
        include_directories(${CMAKE_INSTALL_PREFIX}/include)
        link_directories(${CMAKE_INSTALL_PREFIX}/lib)
    endif()
    include_directories(lwapi)
    add_subdirectory(g_src)
endif()

option(BUILD_MODULES "Build backend modules")
if(BUILD_MODULES)
    add_subdirectory(modules)
endif()

option(BUILD_MODTESTS "Build tests")
if(BUILD_MODTESTS)
    add_subdirectory(modtests)
endif()

if(BUILD_LIBGRAPHICS)
    option(FG_DUMPER "Build FG dumper code" OFF)
    if(FG_DUMPER)
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
        add_library(lwapi ${lwapi_dir}/globals.cc)
        add_dependencies(lwapi generate_code)

    endif()
endif()

