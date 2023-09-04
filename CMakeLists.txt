cmake_minimum_required(VERSION 2.8)
project(embedding_julia)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}")


set(CMAKE_CXX_STANDARD 17) # Use C++17
set(CMAKE_CXX_EXTENSIONS OFF) # Use -std=c++17 instead of something else


# set(Julia_EXECUTABLE "$ENV{HOME}/Desktop/julia/usr/bin/julia-debug")
# set(Julia_LIBRARY_NAME "julia-debug")
find_package(Julia)

add_definitions(${Julia_DEFINITIONS})
include_directories(${Julia_INCLUDE_DIRS})
link_directories(${Julia_LIBRARY_DIRS})


# add_executable(embedding_julia embedding.cpp)
# target_link_libraries(embedding_julia ${Julia_LIBRARIES})

add_executable(sometests sometests.cpp)
target_link_libraries(sometests ${Julia_LIBRARIES})
target_link_libraries(sometests pthread)

# add_executable(julia_multithreading julia_multithreading.cpp)
# target_link_libraries(julia_multithreading ${Julia_LIBRARIES})

# add_executable(juliadispatcher juliadispatcher.cpp)
# target_link_libraries(juliadispatcher ${Julia_LIBRARIES})

# add_executable(julia_cpp_ts julia_cpp_ts.cpp)
# target_link_libraries(julia_cpp_ts ${Julia_LIBRARIES} pthread)
