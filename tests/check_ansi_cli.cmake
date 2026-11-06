cmake_minimum_required(VERSION 3.15)

string(ASCII 27 escape)
foreach(sample IN ITEMS SOURCE ERROR_SOURCE LEXICAL_ERRORS)
    execute_process(
        COMMAND "${PROGRAM}" --ansi "${${sample}}"
        RESULT_VARIABLE exit_code
        OUTPUT_VARIABLE output
        ERROR_VARIABLE diagnostics
    )
    file(READ "${${sample}}" original)
    string(REGEX REPLACE "${escape}\\[[0-9;]*m" "" plain "${output}")
    if(NOT plain STREQUAL original)
        message(FATAL_ERROR "ANSI mode changed source text for ${sample}")
    endif()
    if(NOT output MATCHES "${escape}\\[0m")
        message(FATAL_ERROR "ANSI mode did not emit/reset styles for ${sample}")
    endif()
    if(sample STREQUAL "SOURCE")
        if(NOT exit_code STREQUAL "0" OR NOT diagnostics STREQUAL ""
           OR NOT output MATCHES "${escape}\\[33mwork${escape}\\[0m")
            message(FATAL_ERROR "Valid source lost function coloring or failed: ${diagnostics}")
        endif()
    else()
        if(NOT exit_code STREQUAL "1" OR NOT output MATCHES "${escape}\\[4;31m")
            message(FATAL_ERROR "Invalid source must retain full colored output and exit 1")
        endif()
        if(sample STREQUAL "ERROR_SOURCE")
            string(REGEX MATCHALL "error \\[PAR001\\]" errors "${diagnostics}")
            set(expected_count 5)
        else()
            string(REGEX MATCHALL "error \\[LEX[0-9]+\\]" errors "${diagnostics}")
            set(expected_count 6)
        endif()
        list(LENGTH errors error_count)
        if(NOT error_count EQUAL expected_count OR diagnostics MATCHES "${escape}")
            message(FATAL_ERROR "Diagnostics must remain complete and unstyled on stderr")
        endif()
    endif()
endforeach()

# execute_process(OUTPUT_VARIABLE) normalizes CRLF on Windows. Capture a real
# file instead and compare hex bytes so accidental CRT translation cannot pass.
set(capture_directory "${CMAKE_CURRENT_BINARY_DIR}/ansi_cli")
file(MAKE_DIRECTORY "${capture_directory}")
set(byte_source "${capture_directory}/mixed endings with spaces.bminor")
set(byte_output "${capture_directory}/mixed.ansi")
file(WRITE "${byte_source}" "/* first\r\n\tsecond */\rvalue:\tinteger = 1;  \nnext: boolean = true;")
execute_process(
    COMMAND "${PROGRAM}" --ansi -- "${byte_source}"
    RESULT_VARIABLE byte_exit
    OUTPUT_FILE "${byte_output}"
    ERROR_VARIABLE byte_diagnostics
)
file(READ "${byte_source}" original_hex HEX)
file(READ "${byte_output}" output_hex HEX)
# SGR consists of ESC [, decimal digits/semicolons, then m. This fixture has
# no source-authored escapes, so removing these hex sequences is unambiguous.
string(REGEX REPLACE "1b5b(3[0-9]|3b)*6d" "" plain_hex "${output_hex}")
if(NOT byte_exit STREQUAL "0" OR NOT byte_diagnostics STREQUAL ""
   OR NOT plain_hex STREQUAL original_hex)
    message(FATAL_ERROR "Binary ANSI capture changed CRLF, CR, LF, tabs or trailing bytes")
endif()

file(WRITE "${byte_source}" "")
execute_process(
    COMMAND "${PROGRAM}" --ansi "${byte_source}"
    RESULT_VARIABLE empty_exit
    OUTPUT_FILE "${byte_output}"
    ERROR_VARIABLE empty_diagnostics
)
file(SIZE "${byte_output}" empty_size)
if(NOT empty_exit STREQUAL "0" OR NOT empty_diagnostics STREQUAL "" OR NOT empty_size EQUAL 0)
    message(FATAL_ERROR "Empty source must produce an empty successful ANSI stream")
endif()

foreach(conflict IN ITEMS --tokens --ast --classify --show-whitespace)
    execute_process(
        COMMAND "${PROGRAM}" --ansi "${conflict}" "${SOURCE}"
        RESULT_VARIABLE conflict_exit
        OUTPUT_VARIABLE conflict_output
        ERROR_VARIABLE conflict_diagnostics
    )
    if(NOT conflict_exit STREQUAL "2" OR NOT conflict_output STREQUAL ""
       OR conflict_diagnostics STREQUAL "")
        message(FATAL_ERROR "ANSI conflicting mode/whitespace options must return usage status 2")
    endif()
endforeach()
