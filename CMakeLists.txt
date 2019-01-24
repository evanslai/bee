cmake_minimum_required(VERSION 3.0)

project(bee)

include_directories(includes)
include_directories(external_libs/http-parser)

add_subdirectory(external_libs)

add_library(bee
    bee.c
    bee_http.c
    bee_cli.c
)

add_subdirectory(examples)

