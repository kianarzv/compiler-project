cmake_minimum_required(VERSION 3.15)
project(bminor_frontend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the front-end logic in a library so the CLI and tests use the same code.
add_library(bminor_core
    src/source_file.cpp
    src/token.cpp
    src/lexer.cpp
    src/diagnostic.cpp
    src/ast.cpp
    src/parser.cpp
    src/text_output.cpp
    src/syntax_classifier.cpp
    src/ansi_renderer.cpp
)
target_include_directories(bminor_core PUBLIC include)

add_executable(bminor src/main.cpp src/console_output.cpp)
target_link_libraries(bminor PRIVATE bminor_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(lexer_tests tests/lexer_tests.cpp)
    target_link_libraries(lexer_tests PRIVATE bminor_core)
    add_test(NAME lexer_tests COMMAND lexer_tests)
    add_executable(diagnostic_tests tests/diagnostic_tests.cpp)
    target_link_libraries(diagnostic_tests PRIVATE bminor_core)
    add_test(NAME diagnostic_tests COMMAND diagnostic_tests)
    add_executable(parser_tests tests/parser_tests.cpp)
    target_link_libraries(parser_tests PRIVATE bminor_core)
    add_test(NAME parser_tests COMMAND parser_tests)
    add_executable(expression_tests tests/expression_tests.cpp)
    target_link_libraries(expression_tests PRIVATE bminor_core)
    add_test(NAME expression_tests COMMAND expression_tests)
    add_executable(statement_tests tests/statement_tests.cpp)
    target_link_libraries(statement_tests PRIVATE bminor_core)
    add_test(NAME statement_tests COMMAND statement_tests)
    add_executable(recovery_tests tests/recovery_tests.cpp)
    target_link_libraries(recovery_tests PRIVATE bminor_core)
    add_test(NAME recovery_tests COMMAND recovery_tests)
    add_executable(classifier_tests tests/classifier_tests.cpp)
    target_link_libraries(classifier_tests PRIVATE bminor_core)
    add_test(NAME classifier_tests COMMAND classifier_tests)
    add_executable(ansi_tests tests/ansi_tests.cpp)
    target_link_libraries(ansi_tests PRIVATE bminor_core)
    add_test(NAME ansi_tests COMMAND ansi_tests)
    add_test(NAME lexical_cli_recovery
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:bminor>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/lexical_errors.bminor
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_lexical_cli.cmake
    )
    add_test(NAME declaration_ast_cli
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:bminor>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/declarations.bminor
            -DLEXICAL_ERRORS=${CMAKE_CURRENT_SOURCE_DIR}/examples/lexical_errors.bminor
            -DSYNTAX_ERRORS=${CMAKE_CURRENT_SOURCE_DIR}/examples/declaration_error.bminor
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_ast_cli.cmake
    )
    add_test(NAME expression_ast_cli
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:bminor>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/expressions.bminor
            -DERROR_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/expression_error.bminor
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_expression_cli.cmake
    )
    add_test(NAME statement_ast_cli
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:bminor>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/statements.bminor
            -DHELLO=${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.bminor
            -DLEXER_SAMPLE=${CMAKE_CURRENT_SOURCE_DIR}/examples/lexer_full.bminor
            -DERROR_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/statement_error.bminor
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_statement_cli.cmake
    )
    add_test(NAME syntax_recovery_cli
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:bminor>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/syntax_errors.bminor
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_recovery_cli.cmake
    )
    set_tests_properties(recovery_tests syntax_recovery_cli PROPERTIES TIMEOUT 30)
    add_test(NAME syntax_classification_cli
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:bminor>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/classification.bminor
            -DERROR_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/syntax_errors.bminor
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_classification_cli.cmake
    )
    add_test(NAME ansi_cli
        COMMAND ${CMAKE_COMMAND}
            -DPROGRAM=$<TARGET_FILE:bminor>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/classification.bminor
            -DERROR_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/examples/syntax_errors.bminor
            -DLEXICAL_ERRORS=${CMAKE_CURRENT_SOURCE_DIR}/examples/lexical_errors.bminor
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_ansi_cli.cmake
    )
endif()

foreach(target IN ITEMS
    bminor_core bminor lexer_tests diagnostic_tests parser_tests
    expression_tests statement_tests recovery_tests classifier_tests ansi_tests
)
    if(NOT TARGET ${target})
        continue()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
