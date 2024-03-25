cmake_minimum_required(VERSION 3.22)
set(CC gcc)
set(CCX g++)

project(ctream)

# Enable profiling
# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -pg)
# set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} -pg)
# set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} -pg)

set(includes "include")
file(GLOB_RECURSE tests "test/test*.cpp" "test/**/test*.cpp")

find_package(Catch2 REQUIRED)

# Unit tests
add_executable(test_ctream ${tests})
set_property(TARGET test_ctream PROPERTY CXX_STANDARD 11)
target_link_libraries(test_ctream PRIVATE Catch2::Catch2WithMain)
target_include_directories(test_ctream PRIVATE ${includes})

# Benchmarks
add_executable(benchmark_ctream test/benchmarks.cpp)
target_link_libraries(benchmark_ctream PRIVATE Catch2::Catch2WithMain)
target_include_directories(benchmark_ctream PRIVATE ${includes})

# Examples
include_directories("include")
add_executable(maxInt examples/maxInt.cpp)

