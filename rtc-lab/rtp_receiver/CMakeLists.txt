cmake_minimum_required(VERSION 2.8)

project(rtp_receiver)

if (CMAKE_VERSION VERSION_LESS "3")
    add_definitions(-std=c++11)
else ()
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif ()

find_package(PkgConfig REQUIRED)

pkg_check_modules(OPUS REQUIRED opus)

include_directories(${OPUS_INCLUDE_DIRS})

add_executable(rtp_receiver main.cpp)

target_link_libraries(rtp_receiver OPUS)
