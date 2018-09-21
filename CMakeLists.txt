cmake_minimum_required(VERSION 3.0.0)
project(cstream VERSION 0.1.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")

include(CTest)
enable_testing()

set(SRCS stream.c)
set(HEADERS defs.h stream.h)

add_library(cstream ${SRCS} ${HEADERS})

add_executable(main main.c)
target_link_libraries(main cstream)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
