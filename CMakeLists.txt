cmake_minimum_required(VERSION 3.15)
project(bminor_frontend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(bminor_core src/source_file.cpp)
target_include_directories(bminor_core PUBLIC include)

add_executable(bminor src/main.cpp)
target_link_libraries(bminor PRIVATE bminor_core)

foreach(target bminor_core bminor)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
