cmake_minimum_required(VERSION 3.15)
project(bminor_frontend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the front-end logic in a library so the CLI and tests use the same code.
add_library(bminor_core src/source_file.cpp src/token.cpp src/lexer.cpp)
target_include_directories(bminor_core PUBLIC include)

add_executable(bminor src/main.cpp)
target_link_libraries(bminor PRIVATE bminor_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(lexer_tests tests/lexer_tests.cpp)
    target_link_libraries(lexer_tests PRIVATE bminor_core)
    add_test(NAME lexer_tests COMMAND lexer_tests)
endif()

foreach(target bminor_core bminor lexer_tests)
    if(NOT TARGET ${target})
        continue()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
