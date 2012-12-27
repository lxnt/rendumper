cmake_minimum_required(VERSION 2.8)
project(dfmodglue)

set(DF_MODULES_PATH "libs/" CACHE STRING "module search path")

add_definitions(-DDF_MODULE_PATH="${DF_MODULES_PATH}")
add_library(glue STATIC glue.cpp)
