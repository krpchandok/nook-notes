cmake_minimum_required(VERSION 3.20)
project(nook_notes CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(engine
    engine/src/memtable.cpp
    engine/src/sstable.cpp
    engine/src/compaction.cpp
    engine/src/db.cpp
)
target_include_directories(engine PUBLIC engine/include)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(engine_tests
    engine/tests/memtable_test.cpp
    engine/tests/sstable_test.cpp
    engine/tests/db_test.cpp
)

target_link_libraries(engine_tests PRIVATE engine GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(engine_tests)