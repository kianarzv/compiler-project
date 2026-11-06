execute_process(
    COMMAND "${PROGRAM}" --ast "${SOURCE}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE ast_output
    ERROR_VARIABLE diagnostics
)
if(NOT exit_code STREQUAL "0" OR NOT diagnostics STREQUAL "")
    message(FATAL_ERROR "Expression sample failed: ${exit_code}: ${diagnostics}")
endif()
foreach(label "BinaryExpr" "UnaryExpr" "GroupExpr" "IdentifierExpr" "CallExpr" "IndexExpr" "PostfixExpr")
    if(NOT ast_output MATCHES "${label}")
        message(FATAL_ERROR "Expression AST is missing ${label}")
    endif()
endforeach()

# The first complete declaration is retained, but an incomplete call must not
# be accepted or consume the following declaration as another argument.
execute_process(
    COMMAND "${PROGRAM}" --ast "${ERROR_SOURCE}"
    RESULT_VARIABLE error_exit
    OUTPUT_VARIABLE partial_ast
    ERROR_VARIABLE error_output
)
if(NOT error_exit STREQUAL "1" OR NOT partial_ast MATCHES "Identifier \"ready\""
   OR partial_ast MATCHES "unreached"
   OR NOT error_output MATCHES "expression_error.bminor:2:26: error \\[PAR001\\]")
    message(FATAL_ERROR "Malformed-call diagnostics or partial AST are incorrect: ${error_output}")
endif()
