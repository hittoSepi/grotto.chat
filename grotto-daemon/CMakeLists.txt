cmake_minimum_required(VERSION 3.20)

project(grotto-daemon VERSION 0.1.0 DESCRIPTION "Grotto daemon mascot library" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(grotto-daemon STATIC
    src/grotto.cpp
)

target_include_directories(grotto-daemon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(grotto-daemon PRIVATE /W4 /WX-)
else()
    target_compile_options(grotto-daemon PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(grotto-daemon-demo
    examples/demo.cpp
)

target_link_libraries(grotto-daemon-demo PRIVATE
    grotto-daemon
)
