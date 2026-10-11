cmake_minimum_required(VERSION 3.16)
string( REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

#add_compile_options(-march=native -fopt-info-vec -fopt-info-vec-missed)
#add_compile_options(-march=native -fopt-info-vec)

project(psx)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

add_subdirectory(psx)
add_subdirectory(psx-qt)

