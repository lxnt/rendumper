cmake_minimum_required(VERSION 2.8)
project(rendumper)

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "build type" FORCE)
set(WNO_SIGN_COMPARE "-Wno-sign-compare" CACHE STRING "what it says")

set(CONFIGURING_RENDUMPER TRUE)
add_subdirectory(modules)
include_directories(modules/include)

if(NOT UNIX)
    message(FATAL_ERROR "~")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH64 TRUE)
else()
  set(ARCH64 FALSE)
endif()

set(M32 "-m32 -march=i686")

set(CMAKE_CXX_COMPILER g++-4.5)
set(CMAKE_C_COMPILER g++-4.5)
set(BRING_EM_ON "-Wall -Wextra -pedantic ${WNO_SIGN_COMPARE}")

set(CMAKE_C_FLAGS "${BRING_EM_ON}")
set(CMAKE_C_FLAGS_DEBUG "-ggdb3")
set(CMAKE_CXX_FLAGS "-std=c++0x ${BRING_EM_ON}")
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb3")

if (ARCH64)
    set(CMAKE_LIBRARY_PATH /usr/lib32 /usr/lib/i386-linux-gnu)
    set(CMAKE_C_FLAGS "${M32} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${M32} ${CMAKE_CXX_FLAGS}")
endif()

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
    add_library(lwapi ${lwapi_dir}/globals.cc )
    add_dependencies(lwapi generate_code)

    include_directories(${rendumper_SOURCE_DIR}/lwapi)
endif()

add_subdirectory(g_src)
