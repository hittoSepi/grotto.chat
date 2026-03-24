cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(grotto-server-tui VERSION 0.1.0 DESCRIPTION "Grotto Server TUI" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    add_compile_definitions(BOOST_ALL_NO_LIB)
endif()

# ── FetchContent (FTXUI only) ─────────────────
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
    GIT_SHALLOW    TRUE
)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# ── Dependencies from parent/vcpkg ────────────
if(NOT TARGET Boost::system)
    find_package(Boost REQUIRED COMPONENTS system)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json CONFIG REQUIRED)
endif()

if(NOT TARGET spdlog::spdlog)
    find_package(spdlog CONFIG REQUIRED)
endif()

# ── Static library ────────────────────────────
set(TUI_SOURCES
    src/admin_tui.cpp
    src/admin_protocol.cpp
    src/admin_socket_client.cpp
    src/admin_socket_listener.cpp
    src/log_view.cpp
    src/user_view.cpp
    src/settings_view.cpp
    src/bugreport_view.cpp
)

add_library(grotto-server-tui STATIC ${TUI_SOURCES})

target_include_directories(grotto-server-tui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(grotto-server-tui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(grotto-server-tui PUBLIC
    ftxui::screen
    ftxui::dom
    ftxui::component
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Boost::system
)

if(WIN32)
    target_link_libraries(grotto-server-tui PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(grotto-server-tui PRIVATE /W4 /WX- /FS)
else()
    target_compile_options(grotto-server-tui PRIVATE -Wall -Wextra)
endif()

# ── Standalone executable ─────────────────────
add_executable(grotto-tui src/main.cpp)
target_link_libraries(grotto-tui PRIVATE grotto-server-tui)

# ── Tests ─────────────────────────────────────
option(GROTTO_CHAT_TUI_BUILD_TESTS "Build TUI tests" ON)
if(GROTTO_CHAT_TUI_BUILD_TESTS)
    enable_testing()
    add_executable(test-protocol tests/test_protocol.cpp)
    target_link_libraries(test-protocol PRIVATE grotto-server-tui)
    add_test(NAME protocol-tests COMMAND test-protocol)
endif()
