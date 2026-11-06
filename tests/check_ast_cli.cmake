# A real declaration parse must produce a useful tree and no diagnostics.
execute_process(
    COMMAND "${PROGRAM}" --ast "${SOURCE}"
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE ast_output
    ERROR_VARIABLE diagnostics
)
if(NOT exit_code STREQUAL "0" OR NOT diagnostics STREQUAL "")
    message(FATAL_ERROR "Expected a clean AST parse: ${exit_code}: ${diagnostics}")
endif()

# Completed declarations survive the first syntax error, but later declarations
# are not parsed until panic-mode recovery is implemented in step 8.
execute_process(
    COMMAND "${PROGRAM}" --ast "${SYNTAX_ERRORS}"
    RESULT_VARIABLE syntax_exit
    OUTPUT_VARIABLE partial_ast
    ERROR_VARIABLE syntax_diagnostics
)
if(NOT syntax_exit STREQUAL "1" OR NOT partial_ast MATCHES "Identifier \"ready\""
   OR partial_ast MATCHES "unreached" OR NOT syntax_diagnostics MATCHES "PAR001")
    message(FATAL_ERROR "Partial AST or first-syntax-error behavior is incorrect")
endif()
foreach(label "Program" "VarDecl" "FuncDecl prototype" "ParamDecl" "ArrayInit" "Type array unsized")
    if(NOT ast_output MATCHES "${label}")
        message(FATAL_ERROR "AST output is missing ${label}")
    endif()
endforeach()
if(NOT ast_output MATCHES "declarations.bminor:2:1 type=null")
    message(FATAL_ERROR "AST output is missing the declaration's location/annotation")
endif()

# Lexical failures must be shown before starting the parser, preventing duplicate
# syntax diagnostics and output that could be mistaken for a successful parse.
execute_process(
    COMMAND "${PROGRAM}" --ast "${LEXICAL_ERRORS}"
    RESULT_VARIABLE error_exit
    OUTPUT_VARIABLE error_ast
    ERROR_VARIABLE lexical_diagnostics
)
if(NOT error_exit STREQUAL "1" OR NOT error_ast STREQUAL ""
   OR NOT lexical_diagnostics MATCHES "LEX002"
   OR lexical_diagnostics MATCHES "PAR00")
    message(FATAL_ERROR "AST mode did not respect the lexical error boundary")
endif()
