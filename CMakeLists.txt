cmake_minimum_required(VERSION 3.16)

project(psx)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

add_subdirectory(psx)
add_subdirectory(psx-qt)

