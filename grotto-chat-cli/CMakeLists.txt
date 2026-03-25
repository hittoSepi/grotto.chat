cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(grotto-cli VERSION 0.1.0 DESCRIPTION "Grotto CLI Management Tool" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    add_compile_definitions(BOOST_ALL_NO_LIB)
endif()

# ── grotto-server-tui (AdminSocketClient, protocol) ──
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../grotto-chat-server-tui
                 ${CMAKE_CURRENT_BINARY_DIR}/grotto-chat-server-tui)

# ── Sources ──────────────────────────────────────────
set(CLI_SOURCES
    src/main.cpp
    src/command_registry.cpp
    src/command_context.cpp
    src/commands/help.cpp
    src/commands/version.cpp
    src/commands/status.cpp
    src/commands/users.cpp
    src/commands/shutdown.cpp
    src/commands/restart.cpp
    src/commands/kick.cpp
    src/commands/ban.cpp
    src/commands/logtail.cpp
    src/commands/tui.cpp
    src/commands/update.cpp
)

add_executable(grotto ${CLI_SOURCES})

target_include_directories(grotto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(grotto PRIVATE
    grotto-server-tui
)

if(MSVC)
    target_compile_options(grotto PRIVATE /W4 /WX- /FS)
else()
    target_compile_options(grotto PRIVATE -Wall -Wextra)
endif()

install(TARGETS grotto RUNTIME DESTINATION bin)
