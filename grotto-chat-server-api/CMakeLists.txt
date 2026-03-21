cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(grotto-server-api 
    VERSION 0.1.0 
    DESCRIPTION "Grotto HTTP API Server Library" 
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    add_compile_definitions(BOOST_ALL_NO_LIB)
endif()

set(Boost_NO_BOOST_CMAKE ON)

# ──────────────────────────────────────────────
# Find dependencies (use parent's if available)
# ──────────────────────────────────────────────

# Try to find dependencies from parent project first
if(NOT TARGET Boost::system)
    find_package(boost_system CONFIG QUIET)
endif()

if(NOT TARGET Boost::system)
    find_package(Boost REQUIRED COMPONENTS system)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json CONFIG REQUIRED)
endif()

if(NOT TARGET spdlog::spdlog)
    find_package(spdlog CONFIG REQUIRED)
endif()

find_package(OpenSSL REQUIRED)

# ──────────────────────────────────────────────
# grotto-server-api static library
# ──────────────────────────────────────────────

set(API_SOURCES
    src/server.cpp
    src/router.cpp
    src/middleware.cpp
    src/auth.cpp
    src/request.cpp
    src/response.cpp
    src/types.cpp
)

set(API_HEADERS
    include/grotto/api/server.hpp
    include/grotto/api/request.hpp
    include/grotto/api/response.hpp
    include/grotto/api/router.hpp
    include/grotto/api/middleware.hpp
    include/grotto/api/auth.hpp
    include/grotto/api/types.hpp
)

add_library(grotto-server-api STATIC ${API_SOURCES} ${API_HEADERS})

target_include_directories(grotto-server-api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against Boost.Beast (header-only) and other deps
target_link_libraries(grotto-server-api PUBLIC
    Boost::system
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(grotto-server-api PRIVATE ws2_32)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(grotto-server-api PRIVATE /W4 /WX-)
else()
    target_compile_options(grotto-server-api PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ──────────────────────────────────────────────
# Tests
# ──────────────────────────────────────────────
option(GROTTO_CHAT_API_BUILD_TESTS "Build HTTP API tests" ON)
if(GROTTO_CHAT_API_BUILD_TESTS)
    enable_testing()
    
    add_executable(test-api-server tests/test_server.cpp)
    target_link_libraries(test-api-server PRIVATE grotto-server-api)
    add_test(NAME api-server-tests COMMAND test-api-server)

    add_executable(test-api-router tests/test_router.cpp)
    target_link_libraries(test-api-router PRIVATE grotto-server-api)
    add_test(NAME api-router-tests COMMAND test-api-router)
endif()

# ──────────────────────────────────────────────
# Installation
# ──────────────────────────────────────────────
install(TARGETS grotto-server-api
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/grotto/api
    DESTINATION include/grotto
)
