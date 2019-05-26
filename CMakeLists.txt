cmake_minimum_required(VERSION 3.13)
project(Zadanie2)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lboost_program_options -std=c++17 -pthread")
FIND_PACKAGE(Boost COMPONENTS program_options filesystem REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})

add_executable(netstore-client Client.cpp Client.h)
add_executable(netstore-server Server.cpp Server.h)

TARGET_LINK_LIBRARIES(netstore-client ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(netstore-server ${Boost_LIBRARIES})

