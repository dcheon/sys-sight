# Runs the example program, optionally under the tracer, and checks that it
# did not crash and printed a consistent table.
# Usage: cmake -DEXE=<program> [-DTRACER=<sys-sight>] -P check_output.cmake

execute_process(COMMAND ${TRACER} ${EXE}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "exited with '${rc}'\n${out}${err}")
endif()

foreach(row
    "g_rodata +3 +\\.rodata"
    "g_data +1 +\\.data"
    "g_bss +0 +\\.bss"
    "\\*heap_int +5 +heap"
    "\\*mapped_int +6 +mmap"
    "in_main +4 +stack")
    if(NOT out MATCHES "\n${row} +0x[0-9a-f]+")
        message(FATAL_ERROR "missing or wrong row: ${row}\n${out}")
    endif()
endforeach()

# A pointer's value must equal the address of the variable it points to.
foreach(p heap_int mapped_int)
    string(REGEX MATCH "\n\\*${p} +[0-9]+ +[a-z]+ +(0x[0-9a-f]+)" _ "${out}")
    set(target_address "${CMAKE_MATCH_1}")
    string(REGEX MATCH "\n${p} +(0x[0-9a-f]+) +stack +0x[0-9a-f]+ +\\*${p}" _ "${out}")
    if(target_address STREQUAL "" OR NOT CMAKE_MATCH_1 STREQUAL target_address)
        message(FATAL_ERROR "${p} does not point to *${p} (${target_address} vs ${CMAKE_MATCH_1})\n${out}")
    endif()
endforeach()

if(TRACER AND NOT out MATCHES "\"exit\":{\"status\":0}")
    message(FATAL_ERROR "tracer did not report a clean exit\n${out}")
endif()

if(TRACER)
    string(REGEX MATCHALL "\"snapshot\":" snapshots "${out}")
    list(LENGTH snapshots snapshot_count)
    if(NOT snapshot_count EQUAL 1)
        message(FATAL_ERROR "expected exactly one snapshot, got ${snapshot_count}\n${out}")
    endif()

    string(REGEX MATCH
        "\"rip\":\"(0x[0-9a-f]+)\",\"rsp\":\"(0x[0-9a-f]+)\",\"rbp\":\"(0x[0-9a-f]+)\"" _ "${out}")
    set(snapshot_rsp "${CMAKE_MATCH_2}")
    set(snapshot_rbp "${CMAKE_MATCH_3}")
    string(REGEX MATCH "\nin_main +[0-9]+ +stack +(0x[0-9a-f]+)" _ "${out}")
    set(in_main_address "${CMAKE_MATCH_1}")
    if(snapshot_rsp STREQUAL "" OR snapshot_rbp STREQUAL "" OR in_main_address STREQUAL "")
        message(FATAL_ERROR "snapshot registers or in_main address not found\n${out}")
    endif()

    # in_main lives in main's frame: above the marker call's rsp, below main's rbp.
    math(EXPR above_rsp "${in_main_address} - ${snapshot_rsp}" OUTPUT_FORMAT DECIMAL)
    math(EXPR below_rbp "${snapshot_rbp} - ${in_main_address}" OUTPUT_FORMAT DECIMAL)
    if(NOT above_rsp GREATER 0 OR NOT below_rbp GREATER 0)
        message(FATAL_ERROR
            "in_main ${in_main_address} is outside rsp ${snapshot_rsp} .. rbp ${snapshot_rbp}\n${out}")
    endif()

    # Every variable must sit in the snapshot region of the kind the program claims.
    string(REGEX MATCH "\n(\\{\"snapshot\":[^\n]*)" _ "${out}")
    set(snapshot_json "${CMAKE_MATCH_1}")
    string(JSON region_count LENGTH "${snapshot_json}" snapshot regions)
    if(region_count EQUAL 0)
        message(FATAL_ERROR "snapshot has no regions\n${out}")
    endif()

    function(region_kind address out_var)
        math(EXPR last "${region_count} - 1")
        foreach(index RANGE ${last})
            string(JSON start GET "${snapshot_json}" snapshot regions ${index} start)
            string(JSON end GET "${snapshot_json}" snapshot regions ${index} end)
            math(EXPR from_start "${address} - ${start}" OUTPUT_FORMAT DECIMAL)
            math(EXPR to_end "${end} - ${address}" OUTPUT_FORMAT DECIMAL)
            if(NOT from_start LESS 0 AND to_end GREATER 0)
                string(JSON kind GET "${snapshot_json}" snapshot regions ${index} kind)
                set(${out_var} "${kind}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
        set(${out_var} "none" PARENT_SCOPE)
    endfunction()

    # ASan replaces malloc, so its heap is an mmap region rather than [heap].
    if(NOT DEFINED HEAP_KIND)
        set(HEAP_KIND heap)
    endif()
    foreach(spec
        "g_rodata:rodata"
        "g_data:data"
        "g_bss:data"
        "\\*heap_int:${HEAP_KIND}"
        "\\*mapped_int:mmap"
        "in_main:stack"
        "heap_int:stack"
        "mapped_int:stack")
        string(REGEX MATCH "^(.*):([a-z]+)$" _ "${spec}")
        set(variable "${CMAKE_MATCH_1}")
        set(expected_kind "${CMAKE_MATCH_2}")
        string(REGEX MATCH "\n${variable} +[^ ]+ +[^ ]+ +(0x[0-9a-f]+)" _ "${out}")
        set(variable_address "${CMAKE_MATCH_1}")
        region_kind("${variable_address}" actual_kind)
        if(NOT actual_kind STREQUAL expected_kind)
            message(FATAL_ERROR
                "${variable} at ${variable_address}: expected region '${expected_kind}', got '${actual_kind}'\n${snapshot_json}")
        endif()
    endforeach()
endif()
