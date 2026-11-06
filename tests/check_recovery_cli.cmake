execute_process(
    COMMAND "${PROGRAM}" --ast "${SOURCE}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE ast_output
    ERROR_VARIABLE diagnostic_output
)
if(NOT exit_code STREQUAL "1")
    message(FATAL_ERROR "Recovered syntax errors must still return 1: ${exit_code}")
endif()
string(REGEX MATCHALL "error \\[PAR001\\]" errors "${diagnostic_output}")
list(LENGTH errors error_count)
if(NOT error_count EQUAL 5)
    message(FATAL_ERROR "Expected five independent syntax errors: ${diagnostic_output}")
endif()
foreach(label "ErrorDecl" "ErrorStmt" "FuncDecl definition" "IfStmt else=yes" "ReturnStmt"
              "Identifier \"after\"" "else survives")
    if(NOT ast_output MATCHES "${label}")
        message(FATAL_ERROR "Recovered AST is missing ${label}")
    endif()
endforeach()
if(ast_output MATCHES "discarded malformed loop" OR ast_output MATCHES "error \\[PAR")
    message(FATAL_ERROR "Recovery leaked discarded loop body or diagnostics into the AST")
endif()
