cmake_minimum_required(VERSION 3.13)
project(Zadanie2)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "-lboost_program_options")
FIND_PACKAGE(Boost COMPONENTS program_options filesystem REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})

add_executable(client Client.cpp Client.h)
add_executable(server Server.cpp Server.h)

TARGET_LINK_LIBRARIES(client ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(server ${Boost_LIBRARIES})

