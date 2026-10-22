cmake_minimum_required(VERSION 3.20)
project(sys_sight VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SYS_SIGHT_BUILD_TESTS "Build unit tests" ON)
option(SYS_SIGHT_ENABLE_address "Enable AddressSanitizer" OFF)
option(SYS_SIGHT_ENABLE_undefined "Enable UndefinedBehaviorSanitizer" OFF)

if(SYS_SIGHT_ENABLE_address)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(SYS_SIGHT_ENABLE_undefined)
    add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=undefined)
endif()

add_library(sys_sight_core
    src/state/fd_table.cpp
    src/state/memory_map.cpp
    src/state/process_state.cpp
    src/syscall/syscall_decoder_x86_64.cpp
    src/tracer/ptrace_tracer.cpp
    src/output/jsonl_writer.cpp
)
target_include_directories(sys_sight_core PUBLIC src)
target_compile_options(sys_sight_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(sys-sight src/cli/main.cpp)
target_link_libraries(sys-sight PRIVATE sys_sight_core)
target_compile_options(sys-sight PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(SYS_SIGHT_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(sys_sight_tests
        tests/unit/fd_table_test.cpp
        tests/unit/memory_map_test.cpp
        tests/unit/process_state_test.cpp
        tests/unit/syscall_decoder_test.cpp
    )
    target_link_libraries(sys_sight_tests PRIVATE sys_sight_core GTest::gtest_main)
    target_compile_options(sys_sight_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    include(GoogleTest)
    gtest_discover_tests(sys_sight_tests)
endif()
