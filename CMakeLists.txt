cmake_minimum_required(VERSION 3.0)

project(bee)

include_directories(includes)
include_directories(external_libs/http-parser)
include_directories(external_libs/inih)
include_directories(external_libs/parson)

add_subdirectory(external_libs)

add_library(bee
    bee.c
    bee_http.c
    bee_telnet.c
)

add_subdirectory(examples)

