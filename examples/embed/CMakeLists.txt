cmake_minimum_required(VERSION 3.20)
project(PrimeStructEmbedExample LANGUAGES CXX)

# Consumes an installed PrimeStruct:
#   cmake --install <primestruct-build> --prefix <prefix>
#   cmake -S examples/embed -B build -DCMAKE_PREFIX_PATH=<prefix>
find_package(PrimeStruct REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(embed_example main.cpp)
target_link_libraries(embed_example PRIVATE PrimeStruct::embed)

# Runtime-only host: no compiler code, runs precompiled bytecode (iOS-style).
add_executable(embed_bytecode_runner bytecode_runner.cpp)
target_link_libraries(embed_bytecode_runner PRIVATE PrimeStruct::embed_runtime)
