cmake_minimum_required(VERSION 2.8)

project(HmSearch)

if (NOT CMAKE_BUILD_TYPE)
  # set(CMAKE_BUILD_TYPE "Debug")
  set(CMAKE_BUILD_TYPE Release)
endif ()

# SDSL
link_directories($ENV{HOME}/lib)
include_directories($ENV{HOME}/include)

set(GCC_WARNINGS "-Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pthread ${GCC_WARNINGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer -O0 -g -DDEBUG")

message(STATUS "BUILD_TYPE is ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX_FLAGS are ${CMAKE_CXX_FLAGS}")
message(STATUS "CXX_FLAGS_DEBUG are ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CXX_FLAGS_RELEASE are ${CMAKE_CXX_FLAGS_RELEASE}")

add_executable(main main.cpp)
target_link_libraries(main sdsl)

file(COPY ${CMAKE_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
