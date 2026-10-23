cmake_minimum_required(VERSION 3.20)
project(sys_sight VERSION 0.1.0 LANGUAGES C CXX)

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
    src/elf/elf_reader.cpp
    src/tracer/tracee_memory.cpp
    src/state/fd_table.cpp
    src/state/memory_map.cpp
    src/state/process_state.cpp
    src/state/proc_bootstrap.cpp
    src/state/proc_maps.cpp
    src/state/region_classifier.cpp
    src/syscall/syscall_decoder_x86_64.cpp
    src/tracer/ptrace_tracer.cpp
    src/output/jsonl_writer.cpp
)
target_include_directories(sys_sight_core PUBLIC src include)
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
        tests/unit/proc_bootstrap_test.cpp
        tests/unit/region_classifier_test.cpp
        tests/unit/syscall_decoder_test.cpp
        tests/unit/elf_reader_test.cpp
        tests/unit/tracee_memory_test.cpp
    )
    target_link_libraries(sys_sight_tests PRIVATE sys_sight_core GTest::gtest_main)
    target_compile_definitions(sys_sight_tests PRIVATE
        EXAMPLE_BINARY="$<TARGET_FILE:single_thread_single_function_test>")
    add_dependencies(sys_sight_tests single_thread_single_function_test)
    target_compile_options(sys_sight_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    include(GoogleTest)
    gtest_discover_tests(sys_sight_tests)

    # Example target program, built with the flags sys-sight requires.
    add_executable(single_thread_single_function_test
        examples/single_thread_single_function_test/main.c)
    target_include_directories(single_thread_single_function_test PRIVATE include)
    set_target_properties(single_thread_single_function_test PROPERTIES
        C_STANDARD 11 C_EXTENSIONS ON)
    target_compile_options(single_thread_single_function_test PRIVATE
        -O0 -g -fno-pie -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror)
    target_link_options(single_thread_single_function_test PRIVATE -no-pie)

    # The same program built as PIE: sys-sight must warn, not report wrong addresses.
    add_executable(example_pie examples/single_thread_single_function_test/main.c)
    target_include_directories(example_pie PRIVATE include)
    set_target_properties(example_pie PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
    target_compile_options(example_pie PRIVATE
        -O0 -g -fPIE -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror)
    target_link_options(example_pie PRIVATE -pie)
    add_test(NAME tracer_warns_on_pie COMMAND sys-sight $<TARGET_FILE:example_pie>)
    set_tests_properties(tracer_warns_on_pie PROPERTIES
        PASS_REGULAR_EXPRESSION "\"code\":\"pie\""
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")

    set(EXAMPLE_CHECK ${CMAKE_SOURCE_DIR}/tests/example/check_output.cmake)
    add_test(NAME example_runs
        COMMAND ${CMAKE_COMMAND}
            -DEXE=$<TARGET_FILE:single_thread_single_function_test>
            -P ${EXAMPLE_CHECK})
    set(EXAMPLE_HEAP_KIND heap)
    if(SYS_SIGHT_ENABLE_address)
        set(EXAMPLE_HEAP_KIND mmap)
    endif()
    add_test(NAME example_runs_under_tracer
        COMMAND ${CMAKE_COMMAND}
            -DEXE=$<TARGET_FILE:single_thread_single_function_test>
            -DTRACER=$<TARGET_FILE:sys-sight>
            -DHEAP_KIND=${EXAMPLE_HEAP_KIND}
            -P ${EXAMPLE_CHECK})
    # The tracer must report a failing tracee instead of exiting 0.
    add_test(NAME tracer_reports_nonzero_exit COMMAND sys-sight /bin/false)
    set_tests_properties(tracer_reports_nonzero_exit PROPERTIES
        PASS_REGULAR_EXPRESSION "\"exit\":\\{\"status\":1\\}")
    add_test(NAME tracer_reports_signal
        COMMAND sys-sight /bin/sh -c "kill -SEGV $$")
    set_tests_properties(tracer_reports_signal PROPERTIES
        PASS_REGULAR_EXPRESSION "\"exit\":\\{\"signal\":11\\}")
    add_test(NAME tracer_no_snapshot_without_marker COMMAND sys-sight /bin/true)
    set_tests_properties(tracer_no_snapshot_without_marker PROPERTIES
        FAIL_REGULAR_EXPRESSION "\"snapshot\"")
    add_test(NAME tracer_reports_exec_failure
        COMMAND sys-sight /nonexistent_prog)
    set_tests_properties(tracer_reports_exec_failure PROPERTIES
        PASS_REGULAR_EXPRESSION "\"error\":\"exec_failed\",\"errno\":2")
    add_test(NAME tracer_exec_failure_exit_code
        COMMAND sh -c "$<TARGET_FILE:sys-sight> /nonexistent_prog 2>/dev/null; test $? -eq 127")
    # LeakSanitizer aborts the tracee under ptrace, and ASan's fake stack
    # moves locals off the real stack, which the frame check relies on.
    set_tests_properties(example_runs_under_tracer PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:detect_stack_use_after_return=0")
endif()
