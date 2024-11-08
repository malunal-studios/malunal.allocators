cmake_minimum_required(VERSION 3.16)
project(malunal.allocators.example VERSION 1.0.0 LANGUAGES CXX)

add_executable(example testing.cpp)
target_link_libraries(example PRIVATE malunal::allocators)
