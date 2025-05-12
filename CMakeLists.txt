
cmake_minimum_required(VERSION 3.10)
project(distributed_queue)
set(CMAKE_CXX_STANDARD 17)

add_executable(distributed_queue src/main.cpp src/server.cpp)
target_include_directories(distributed_queue PRIVATE ${CMAKE_SOURCE_DIR}/include)
