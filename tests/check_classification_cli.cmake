execute_process(
    COMMAND "${PROGRAM}" --classify "${SOURCE}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE output
    ERROR_VARIABLE diagnostics
)
if(NOT exit_code STREQUAL "0" OR NOT diagnostics STREQUAL "")
    message(FATAL_ERROR "Classification sample failed: ${exit_code}: ${diagnostics}")
endif()
foreach(role "FUNCTION_DECLARATION" "FUNCTION_CALL" "VARIABLE_DECLARATION"
             "PARAMETER_DECLARATION" "VARIABLE_REFERENCE" "STRING_LITERAL" "COMMENT")
    if(NOT output MATCHES "${role}")
        message(FATAL_ERROR "Classification table is missing ${role}")
    endif()
endforeach()
if(output MATCHES "WHITESPACE" OR output MATCHES "error \\[PAR")
    message(FATAL_ERROR "Default table leaked whitespace rows or diagnostics")
endif()
execute_process(
    COMMAND "${PROGRAM}" --classify --show-whitespace "${SOURCE}"
    RESULT_VARIABLE full_exit
    OUTPUT_VARIABLE full_output
)
if(NOT full_exit STREQUAL "0" OR NOT full_output MATCHES "WHITESPACE")
    message(FATAL_ERROR "Classification whitespace display failed")
endif()
execute_process(
    COMMAND "${PROGRAM}" --classify "${ERROR_SOURCE}"
    RESULT_VARIABLE error_exit
    OUTPUT_VARIABLE recovered_output
    ERROR_VARIABLE recovered_diagnostics
)
string(REGEX MATCHALL "error \\[PAR001\\]" errors "${recovered_diagnostics}")
list(LENGTH errors error_count)
if(NOT error_exit STREQUAL "1" OR NOT error_count EQUAL 5
   OR NOT recovered_output MATCHES "VARIABLE_DECLARATION"
   OR NOT recovered_output MATCHES "yes")
    message(FATAL_ERROR "Recovered classification lost roles, decorations or diagnostics")
endif()
execute_process(
    COMMAND "${PROGRAM}" --ast --classify "${SOURCE}"
    RESULT_VARIABLE conflict_exit
    OUTPUT_QUIET ERROR_QUIET
)
if(NOT conflict_exit STREQUAL "2")
    message(FATAL_ERROR "Conflicting output modes must return usage status 2")
endif()
