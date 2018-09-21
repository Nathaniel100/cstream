cmake_minimum_required(VERSION 3.0.0)
project(cstream VERSION 0.1.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")

include(CTest)
enable_testing()

add_library(cstream cstream.c)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
