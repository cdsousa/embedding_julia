cmake_minimum_required(VERSION 2.8)
project(embedding_julia)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}")

add_definitions("-std=c++11")


find_package(Julia)
add_definitions(${Julia_DEFINITIONS})
include_directories(${Julia_INCLUDE_DIRS})
link_directories(${Julia_LIBRARY_DIRS})

add_executable(embedding_julia embedding.cpp)
target_link_libraries(embedding_julia ${Julia_LIBRARIES})


add_executable(sometests sometests.cpp)
target_link_libraries(sometests pthread ${Julia_LIBRARIES})
