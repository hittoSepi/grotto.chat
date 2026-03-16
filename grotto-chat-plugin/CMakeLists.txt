cmake_minimum_required(VERSION 3.20)

project(grotto-plugin VERSION 0.1.0 DESCRIPTION "Grotto Plugin System" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ──────────────────────────────────────────────
# FetchContent
# ──────────────────────────────────────────────
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ── QuickJS-NG (MSVC-compatible fork with CMake) ─────
set(QUICKJS_GIT_URL "https://github.com/quickjs-ng/quickjs.git")
FetchContent_Declare(
    quickjs
    GIT_REPOSITORY "${QUICKJS_GIT_URL}"
    GIT_TAG        v0.9.0
    GIT_SHALLOW    TRUE
)
set(QJS_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(quickjs)

# ── nlohmann/json (header-only) ──────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ── spdlog (logging) ─────────────────────────
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.1
    GIT_SHALLOW    TRUE
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# ──────────────────────────────────────────────
# grotto-plugin static library
# ──────────────────────────────────────────────
set(PLUGIN_SOURCES
    src/plugin_meta.cpp
    src/plugin_instance.cpp
    src/plugin_manager.cpp
    src/js_api.cpp
    src/js_timers.cpp
    src/bot_plugin_instance.cpp
    src/client_ext_plugin.cpp
    src/server_ext_plugin.cpp
)

add_library(grotto-plugin STATIC ${PLUGIN_SOURCES})

target_include_directories(grotto-plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(grotto-plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(grotto-plugin PUBLIC
    qjs
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

if(MSVC)
    target_compile_options(grotto-plugin PRIVATE /W4 /WX-)
else()
    target_compile_options(grotto-plugin PRIVATE -Wall -Wextra)
endif()

# ──────────────────────────────────────────────
# Tests
# ──────────────────────────────────────────────
option(GROTTO_CHAT_PLUGIN_BUILD_TESTS "Build plugin system tests" ON)
if(GROTTO_CHAT_PLUGIN_BUILD_TESTS)
    enable_testing()

    add_executable(test-plugin tests/test_plugin.cpp)
    target_link_libraries(test-plugin PRIVATE grotto-plugin)
    add_test(NAME plugin-tests COMMAND test-plugin)
endif()
