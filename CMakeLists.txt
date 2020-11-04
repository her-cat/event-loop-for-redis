cmake_minimum_required(VERSION 3.10)
project(event-loop-for-redis C)

set(CMAKE_C_STANDARD 99)

include_directories(${PROJECT_SOURCE_DIR})

# check epoll and add config.h for the macro compilation.
include(CheckSymbolExists)

check_symbol_exists(epoll_create "sys/epoll.h" EPOLL_EXISTS)
if (EPOLL_EXISTS)
    message(STATUS "Have epoll")
    add_definitions(-D HAVE_EPOLL)
endif()

check_symbol_exists(poll "sys/poll.h" POLL_EXISTS)
if (POLL_EXISTS)
    message(STATUS "Have poll")
    add_definitions(-D HAVE_POLL)
endif()

check_symbol_exists(select "sys/select.h" SELECT_EXISTS)
if (SELECT_EXISTS)
    message(STATUS "Have select")
    add_definitions(-D HAVE_SELECT)
endif()

add_executable(event-loop-for-redis main.c)
