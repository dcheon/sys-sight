# Runs the locals example under the tracer and checks the local variables in
# the snapshot: names, types, addresses, and decoded values.
# Usage: cmake -DEXE=<program or source> -DTRACER=<sys-sight> [-DTRACER_ARGS=run]
#        -P check_locals.cmake

execute_process(COMMAND ${TRACER} ${TRACER_ARGS} ${EXE}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "exited with '${rc}'\n${out}${err}")
endif()
if(out MATCHES "no_debug_info")
    message(FATAL_ERROR "no debug information found\n${out}")
endif()

string(REGEX MATCH "\n(\\{\"snapshot\":[^\n]*)" _ "${out}")
set(snapshot "${CMAKE_MATCH_1}")

# Sets `variable_json` to the local called `name` in frame `frame`.
function(find_local frame name)
    string(JSON count LENGTH "${snapshot}" snapshot stack frames ${frame} locals)
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
        string(JSON found GET "${snapshot}" snapshot stack frames ${frame} locals ${index} name)
        if(found STREQUAL name)
            string(JSON json GET "${snapshot}" snapshot stack frames ${frame} locals ${index})
            set(variable_json "${json}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "frame ${frame} has no local '${name}'\n${snapshot}")
endfunction()

function(expect_equal what actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${what}: expected '${expected}', got '${actual}'\n${snapshot}")
    endif()
endfunction()

# The program printed the address of `counter` through %p; the snapshot must agree.
string(REGEX MATCH "1 1 42 10 4 (0x[0-9a-f]+)" _ "${out}")
set(printed_pointer "${CMAKE_MATCH_1}")

# Frame 0 is show(), frame 1 is main().
find_local(0 depth)
string(JSON role GET "${variable_json}" role)
string(JSON value GET "${variable_json}" value value)
expect_equal("depth role" "${role}" "parameter")
expect_equal("depth" "${value}" "1")

find_local(0 origin)
string(JSON type GET "${variable_json}" type)
string(JSON y GET "${variable_json}" value fields 1 value value)
expect_equal("origin type" "${type}" "struct point")
expect_equal("origin.y" "${y}" "2")

find_local(0 counter)
string(JSON value GET "${variable_json}" value value)
string(JSON bytes GET "${variable_json}" bytes)
string(JSON counter_address GET "${variable_json}" address)
expect_equal("counter" "${value}" "42")
expect_equal("counter bytes" "${bytes}" "2a000000")

find_local(0 numbers)
string(JSON type GET "${variable_json}" type)
string(JSON third GET "${variable_json}" value elements 2 value)
expect_equal("numbers type" "${type}" "int[3]")
expect_equal("numbers[2]" "${third}" "30")

find_local(0 grid)
string(JSON last_value GET "${variable_json}" value elements 1 elements 1 value)
expect_equal("grid[1][1]" "${last_value}" "4")

find_local(0 square)
string(JSON text GET "${variable_json}" value fields 0 value text)
string(JSON corner_x GET "${variable_json}" value fields 1 value fields 0 value value)
string(JSON color GET "${variable_json}" value fields 2 value name)
string(JSON scale GET "${variable_json}" value fields 3 value value)
string(JSON flags GET "${variable_json}" value fields 4 value value)
string(JSON note_kind GET "${variable_json}" value fields 5 value kind)
string(JSON note_type GET "${variable_json}" value fields 5 type)
expect_equal("square.label" "${text}" "sq")
expect_equal("square.corner.x" "${corner_x}" "3")
expect_equal("square.color" "${color}" "GREEN")
expect_equal("square.scale" "${scale}" "1.5")
expect_equal("square.flags" "${flags}" "5")
expect_equal("square.note kind" "${note_kind}" "pointer")
expect_equal("square.note type" "${note_type}" "const char *")

find_local(0 both)
string(JSON whole GET "${variable_json}" value fields 0 value value)
string(JSON real GET "${variable_json}" value fields 1 value value)
expect_equal("both.whole" "${whole}" "1065353216")
expect_equal("both.real" "${real}" "1")

find_local(0 pointer)
string(JSON pointed GET "${variable_json}" value value)
expect_equal("pointer" "${pointed}" "${counter_address}")
expect_equal("pointer vs printed" "${pointed}" "${printed_pointer}")

# A variable of a nested block that contains the marker call is in scope.
find_local(0 inside_block)
string(JSON value GET "${variable_json}" value value)
expect_equal("inside_block" "${value}" "7")

find_local(1 start)
string(JSON x GET "${variable_json}" value fields 0 value value)
expect_equal("start.x" "${x}" "1")
