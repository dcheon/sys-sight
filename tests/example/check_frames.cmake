# Runs the nested_calls example under the tracer and checks the stack frames
# in the snapshot against the addresses the program printed.
# Usage: cmake -DEXE=<program or source> -DTRACER=<sys-sight> [-DTRACER_ARGS=run]
#        -P check_frames.cmake

execute_process(COMMAND ${TRACER} ${TRACER_ARGS} ${EXE}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "exited with '${rc}'\n${out}${err}")
endif()
if(out MATCHES "\"stack_walk\"")
    message(FATAL_ERROR "tracer reported an incomplete stack walk\n${out}")
endif()

string(REGEX MATCH "\n(\\{\"snapshot\":[^\n]*)" _ "${out}")
set(snapshot_json "${CMAKE_MATCH_1}")
string(JSON frame_count LENGTH "${snapshot_json}" snapshot stack frames)
string(JSON stack_end GET "${snapshot_json}" snapshot stack end)
if(NOT frame_count EQUAL 3 OR NOT stack_end STREQUAL "left_executable")
    message(FATAL_ERROR "expected 3 frames ending at left_executable, got ${frame_count} / ${stack_end}\n${snapshot_json}")
endif()

# Frame 0 is the function that called the marker; each local lives in its own frame.
set(index 0)
foreach(spec "inner:in_inner" "outer:in_outer" "main:in_main")
    string(REGEX MATCH "^(.*):(.*)$" _ "${spec}")
    set(function "${CMAKE_MATCH_1}")
    set(variable "${CMAKE_MATCH_2}")

    string(JSON name GET "${snapshot_json}" snapshot stack frames ${index} function)
    string(JSON start GET "${snapshot_json}" snapshot stack frames ${index} start)
    string(JSON end GET "${snapshot_json}" snapshot stack frames ${index} end)
    string(REGEX MATCH "\n${variable} +[^ ]+ +[^ ]+ +(0x[0-9a-f]+)" _ "${out}")
    set(address "${CMAKE_MATCH_1}")
    if(NOT name STREQUAL function OR address STREQUAL "")
        message(FATAL_ERROR "frame ${index}: expected function ${function}, got '${name}' (address '${address}')\n${snapshot_json}")
    endif()

    math(EXPR from_start "${address} - ${start}" OUTPUT_FORMAT DECIMAL)
    math(EXPR to_end "${end} - ${address}" OUTPUT_FORMAT DECIMAL)
    if(from_start LESS 0 OR NOT to_end GREATER 0)
        message(FATAL_ERROR "${variable} at ${address} is outside frame ${index} ${function} [${start}, ${end})\n${snapshot_json}")
    endif()

    if(index GREATER 0)
        if(NOT start STREQUAL previous_end)
            message(FATAL_ERROR "frame ${index} starts at ${start}, previous frame ended at ${previous_end}")
        endif()
    endif()
    set(previous_end "${end}")
    math(EXPR index "${index} + 1")
endforeach()
