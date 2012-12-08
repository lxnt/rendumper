cmake_minimum_required(VERSION 2.8)
project(rendumper)

set(LIB32_LINK_DIR "/home/lxnt/00DFGL/lib32" CACHE PATH "what it says")
set(DF_LINUX_DIR "/home/lxnt/00DFGL/df3411v" CACHE STRING "df_linux")
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "build type" FORCE)

set(CONFIGURING_RENDUMPER TRUE)
add_subdirectory(modules)
include_directories(modules/include)

if(NOT UNIX)
    message(FATAL_ERROR "~")
else()
    set(CMAKE_CXX_COMPILER g++-4.5)
    set(BRING_EM_ON "-Wall -Wextra -pedantic")
    set(CMAKE_LIBRARY_PATH /usr/lib32 /usr/lib/i386-linux-gnu ${LIB32_LINK_DIR})
    set(CMAKE_CXX_FLAGS_DEBUG "-m32 -g -std=c++0x ${BRING_EM_ON}")
    set(CMAKE_C_FLAGS_DEBUG "-m32 -g ${BRING_EM_ON}")
    set(CMAKE_CXX_FLAGS "-m32 -march=i686 -mtune=generic -std=c++0x  ${BRING_EM_ON}")
    set(CMAKE_C_FLAGS "-m32 -march=i686 -mtune=generic ${BRING_EM_ON}")
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
