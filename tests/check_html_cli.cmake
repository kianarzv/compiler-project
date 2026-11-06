cmake_minimum_required(VERSION 3.15)

set(capture_directory "${CMAKE_CURRENT_BINARY_DIR}/html_cli")
file(MAKE_DIRECTORY "${capture_directory}")
set(destination "${capture_directory}/highlight with spaces.html")
foreach(sample IN ITEMS SOURCE ERROR_SOURCE LEXICAL_ERRORS)
    execute_process(
        COMMAND "${PROGRAM}" --html --output "${destination}" "${${sample}}"
        RESULT_VARIABLE exit_code OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics
    )
    if(NOT EXISTS "${destination}" OR NOT output MATCHES "Saved HTML:")
        message(FATAL_ERROR "HTML mode did not save/report the document: ${diagnostics}")
    endif()
    file(READ "${destination}" document)
    if(NOT document MATCHES "^<!DOCTYPE html>" OR NOT document MATCHES "</html>"
       OR NOT document MATCHES "<style>" OR document MATCHES "<script")
        message(FATAL_ERROR "Expected standalone HTML with embedded CSS and no scripts")
    endif()
    if(sample STREQUAL "SOURCE")
        if(NOT exit_code STREQUAL "0" OR NOT diagnostics STREQUAL ""
           OR NOT document MATCHES "class=\"fn-call\">work</span>")
            message(FATAL_ERROR "Valid HTML export lost AST roles or reported errors: ${diagnostics}")
        endif()
        # A stdout export and binary file export must contain identical bytes.
        execute_process(
            COMMAND "${PROGRAM}" --html "${SOURCE}"
            RESULT_VARIABLE stdout_exit OUTPUT_FILE "${capture_directory}/stdout.html"
            ERROR_VARIABLE stdout_diagnostics
        )
        file(READ "${destination}" file_hex HEX)
        file(READ "${capture_directory}/stdout.html" stdout_hex HEX)
        if(NOT stdout_exit STREQUAL "0" OR NOT stdout_diagnostics STREQUAL ""
           OR NOT stdout_hex STREQUAL file_hex)
            message(FATAL_ERROR "HTML stdout/file output differs or includes status headings")
        endif()
    else()
        if(sample STREQUAL "ERROR_SOURCE")
            string(REGEX MATCHALL "error \\[PAR001\\]" errors "${diagnostics}")
            set(expected_count 5)
        else()
            string(REGEX MATCHALL "error \\[LEX[0-9]+\\]" errors "${diagnostics}")
            set(expected_count 6)
        endif()
        list(LENGTH errors error_count)
        if(NOT exit_code STREQUAL "1" OR NOT error_count EQUAL expected_count
           OR NOT document MATCHES "class=\"[^\"]+ error\""
           OR document MATCHES "error \\[(LEX|PAR)")
            message(FATAL_ERROR "Error export lost full document, decorations, diagnostics or status")
        endif()
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" --html --output "${destination}" "${PREVIEW_SOURCE}"
    RESULT_VARIABLE preview_exit ERROR_VARIABLE preview_diagnostics OUTPUT_QUIET
)
file(READ "${destination}" preview)
if(NOT preview_exit STREQUAL "0" OR NOT preview_diagnostics STREQUAL ""
   OR NOT preview MATCHES "&lt;title&gt;Source text&lt;/title&gt; &amp;"
   OR NOT preview MATCHES "class=\"fn-call\">double_value</span>")
    message(FATAL_ERROR "The HTML preview example lost literal markup or AST function styling")
endif()

# A copied input keeps this destructive-regression check away from examples.
# The alias spelling also proves that source protection is not a string match.
set(source_copy "${capture_directory}/source copy.bminor")
file(WRITE "${source_copy}" "value: integer = 1;\r\n")
file(READ "${source_copy}" original_hex HEX)
execute_process(
    COMMAND "${PROGRAM}" --html --output "${capture_directory}/./source copy.bminor" "${source_copy}"
    RESULT_VARIABLE same_exit OUTPUT_VARIABLE same_output ERROR_VARIABLE same_diagnostics
)
file(READ "${source_copy}" unchanged_hex HEX)
if(NOT same_exit STREQUAL "1" OR NOT unchanged_hex STREQUAL original_hex
   OR NOT same_output STREQUAL "" OR NOT same_diagnostics MATCHES "Refusing to overwrite")
    message(FATAL_ERROR "HTML export must refuse an output containing the source bytes")
endif()

execute_process(
    COMMAND "${PROGRAM}" --html --output "${capture_directory}/missing/preview.html" "${SOURCE}"
    RESULT_VARIABLE missing_parent_exit OUTPUT_VARIABLE missing_parent_output ERROR_VARIABLE missing_parent_error
)
if(NOT missing_parent_exit STREQUAL "1" OR NOT missing_parent_output STREQUAL ""
   OR NOT missing_parent_error MATCHES "Cannot open HTML output")
    message(FATAL_ERROR "Unwritable output must fail without claiming a successful save")
endif()

file(WRITE "${destination}" "keep existing output")
execute_process(
    COMMAND "${PROGRAM}" --html --output "${destination}" "${capture_directory}/missing.bminor"
    RESULT_VARIABLE missing_source_exit OUTPUT_QUIET ERROR_QUIET
)
file(READ "${destination}" untouched)
if(NOT missing_source_exit STREQUAL "1" OR NOT untouched STREQUAL "keep existing output")
    message(FATAL_ERROR "A source read failure must leave existing output untouched")
endif()

foreach(conflict IN ITEMS --tokens --ast --classify --ansi --show-whitespace)
    execute_process(
        COMMAND "${PROGRAM}" --html "${conflict}" "${SOURCE}"
        RESULT_VARIABLE conflict_exit OUTPUT_VARIABLE conflict_output ERROR_QUIET
    )
    if(NOT conflict_exit STREQUAL "2" OR NOT conflict_output STREQUAL "")
        message(FATAL_ERROR "HTML mode conflicts must return usage status 2")
    endif()
endforeach()
foreach(bad_arguments IN ITEMS "--html;--output" "--output;${destination};${SOURCE}"
        "--html;--output;--ansi;${SOURCE}"
        "--html;--output;${destination};--output;${destination};${SOURCE}")
    execute_process(
        COMMAND "${PROGRAM}" ${bad_arguments}
        RESULT_VARIABLE usage_exit OUTPUT_VARIABLE usage_output ERROR_QUIET
    )
    if(NOT usage_exit STREQUAL "2" OR NOT usage_output STREQUAL "")
        message(FATAL_ERROR "Malformed output options must return usage status 2")
    endif()
endforeach()
