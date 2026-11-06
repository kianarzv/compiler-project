# Exercise complete programs through the same CLI used after each development step.
foreach(source "${SOURCE}" "${HELLO}" "${LEXER_SAMPLE}")
    execute_process(
        COMMAND "${PROGRAM}" --ast "${source}"
        RESULT_VARIABLE exit_code
        OUTPUT_VARIABLE ast_output
        ERROR_VARIABLE diagnostics
    )
    if(NOT exit_code STREQUAL "0" OR NOT diagnostics STREQUAL ""
       OR NOT ast_output MATCHES "FuncDecl definition"
       OR NOT ast_output MATCHES "ReturnStmt")
        message(FATAL_ERROR "Complete program failed: ${source}: ${diagnostics}")
    endif()
    if(source STREQUAL "${SOURCE}")
        foreach(label "DeclStmt" "BlockStmt" "IfStmt else=yes" "ForStmt" "PrintStmt"
                      "ExprStmt empty" "ReturnStmt empty" "CallExpr")
            if(NOT ast_output MATCHES "${label}")
                message(FATAL_ERROR "Statement sample is missing ${label}")
            endif()
        endforeach()
    endif()
endforeach()

# Before step 8, the first error discards the incomplete function but retains
# complete top-level declarations before it. The CLI must still return 1.
execute_process(
    COMMAND "${PROGRAM}" --ast "${ERROR_SOURCE}"
    RESULT_VARIABLE error_exit
    OUTPUT_VARIABLE partial_ast
    ERROR_VARIABLE error_output
)
if(NOT error_exit STREQUAL "1" OR NOT partial_ast MATCHES "Identifier \"version\""
   OR partial_ast MATCHES "FuncDecl definition" OR partial_ast MATCHES "Identifier \"later\""
   OR NOT error_output MATCHES "statement_error.bminor:5:1: error \\[PAR001\\]")
    message(FATAL_ERROR "Failed-body diagnostics or partial AST are incorrect: ${error_output}")
endif()
