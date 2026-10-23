# Runs a program under the tracer and requires the memory map rebuilt from
# system calls to match /proc/<pid>/maps at the snapshot.
# Usage: cmake -DEXE=<program or source> -DTRACER=<sys-sight> [-DTRACER_ARGS=run]
#        [-DEXPECT_STACK_GROWTH=ON] -P check_oracle.cmake

execute_process(COMMAND ${TRACER} ${TRACER_ARGS} ${EXE}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "exited with '${rc}'\n${out}${err}")
endif()

string(REGEX MATCH "\n(\\{\"snapshot\":[^\n]*)" _ "${out}")
set(snapshot_json "${CMAKE_MATCH_1}")
if(snapshot_json STREQUAL "")
    message(FATAL_ERROR "no snapshot in the output\n${out}")
endif()

string(JSON compared GET "${snapshot_json}" snapshot oracle compared)
string(JSON matches GET "${snapshot_json}" snapshot oracle matches)
string(JSON mismatch_count GET "${snapshot_json}" snapshot oracle mismatch_count)
if(NOT compared STREQUAL "ON" OR NOT matches STREQUAL "ON" OR NOT mismatch_count EQUAL 0)
    string(JSON oracle GET "${snapshot_json}" snapshot oracle)
    message(FATAL_ERROR "memory map from system calls differs from /proc:\n${oracle}")
endif()

# The program must really have outgrown the initial stack, or the check proves nothing.
if(EXPECT_STACK_GROWTH)
    string(JSON growth GET "${snapshot_json}" snapshot oracle stack_growth_bytes)
    if(NOT growth GREATER 0)
        message(FATAL_ERROR "expected the kernel to grow the stack, stack_growth_bytes is ${growth}")
    endif()
endif()
