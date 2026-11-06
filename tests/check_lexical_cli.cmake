# Exercise the real executable: a recovered scan must print later tokens and
# EOF on stdout, report all six sample errors on stderr, and still return 1.
execute_process(
    COMMAND "${PROGRAM}" --tokens "${SOURCE}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE token_output
    ERROR_VARIABLE diagnostic_output
)

if(NOT exit_code STREQUAL "1")
    message(FATAL_ERROR "Expected lexical error exit code 1, got ${exit_code}")
endif()
if(NOT token_output MATCHES "KW_RETURN" OR NOT token_output MATCHES "EOF")
    message(FATAL_ERROR "Recovered token output is missing return or EOF")
endif()
if(token_output MATCHES "error \\[LEX")
    message(FATAL_ERROR "Diagnostics must be written to stderr")
endif()
string(REGEX MATCHALL "error \\[LEX[0-9]+\\]" errors "${diagnostic_output}")
list(LENGTH errors error_count)
if(NOT error_count EQUAL 6)
    message(FATAL_ERROR "Expected six diagnostics, got ${error_count}: ${diagnostic_output}")
endif()
if(NOT diagnostic_output MATCHES "lexical_errors.bminor:5:20: error \\[LEX002\\]")
    message(FATAL_ERROR "Unfinished string diagnostic has the wrong position or code")
endif()
