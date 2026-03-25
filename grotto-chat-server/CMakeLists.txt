cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

set(PROJECT_NAME "grotto-server")
set(PROJECT_DESCRIPTION "Grotto Dedicated Server")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/functions.cmake")

# Find vcpkg integration before project() so dependencies resolve correctly.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{CMAKE_TOOLCHAIN_FILE} AND NOT "$ENV{CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{CMAKE_TOOLCHAIN_FILE}")
    elseif(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    endif()
endif()

if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    if(DEFINED ENV{VCPKG_TARGET_TRIPLET} AND NOT "$ENV{VCPKG_TARGET_TRIPLET}" STREQUAL "")
        set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_TARGET_TRIPLET}")
    elseif(WIN32)
        set(VCPKG_TARGET_TRIPLET "x64-windows")
    else()
        set(VCPKG_TARGET_TRIPLET "x64-linux")
    endif()

    set(_LOCAL_VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
    set(_GLOBAL_VCPKG_INSTALLED_DIR "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")

    if(EXISTS "${_LOCAL_VCPKG_INSTALLED_DIR}")
        set(_VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed")
        set(VCPKG_INSTALLED_DIR "${_VCPKG_INSTALLED_DIR}")
        list(PREPEND CMAKE_PREFIX_PATH "${_LOCAL_VCPKG_INSTALLED_DIR}")
        set(OPENSSL_ROOT_DIR "${_LOCAL_VCPKG_INSTALLED_DIR}")
        message(STATUS "Using local vcpkg package prefix: ${_LOCAL_VCPKG_INSTALLED_DIR}")
    elseif(EXISTS "${_GLOBAL_VCPKG_INSTALLED_DIR}")
        set(_VCPKG_INSTALLED_DIR "$ENV{VCPKG_ROOT}/installed")
        set(VCPKG_INSTALLED_DIR "${_VCPKG_INSTALLED_DIR}")
        list(PREPEND CMAKE_PREFIX_PATH "${_GLOBAL_VCPKG_INSTALLED_DIR}")
        set(OPENSSL_ROOT_DIR "${_GLOBAL_VCPKG_INSTALLED_DIR}")
        message(STATUS "Using global vcpkg package prefix: ${_GLOBAL_VCPKG_INSTALLED_DIR}")
    endif()
endif()

generate_version()

project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} DESCRIPTION ${PROJECT_DESCRIPTION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    add_compile_definitions(BOOST_ALL_NO_LIB)
endif()

set(Boost_NO_BOOST_CMAKE ON)
cmake_policy(SET CMP0074 NEW)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)

# ──────────────────────────────────────────────
# HTTP API Server (optional)
# ──────────────────────────────────────────────
option(GROTTO_CHAT_ENABLE_HTTP_API "Enable HTTP API server" OFF)

if(GROTTO_CHAT_ENABLE_HTTP_API)
    # Check if grotto-server-api exists as subdirectory
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/grotto-server-api/CMakeLists.txt")
        message(STATUS "HTTP API server: Enabled (using local grotto-server-api)")
        add_subdirectory(grotto-server-api)
        add_compile_definitions(GROTTO_CHAT_HTTP_API_ENABLED)
    else()
        message(WARNING "HTTP API server enabled but grotto-server-api not found!")
        message(WARNING "Clone it: git submodule add https://github.com/hittoSepi/grotto-server-api.git")
        set(GROTTO_CHAT_ENABLE_HTTP_API OFF)
    endif()
else()
    message(STATUS "HTTP API server: Disabled (enable with -DGROTTO_CHAT_ENABLE_HTTP_API=ON)")
endif()
find_package(Protobuf REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(spdlog REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(SQLiteCpp REQUIRED)
find_package(toml11 CONFIG REQUIRED)

set(PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/grotto.proto")
if(NOT Protobuf_PROTOC_EXECUTABLE)
    set(_LOCAL_PROTOC "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/protobuf/protoc")
    set(_LOCAL_PROTOC_WIN "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/protobuf/protoc.exe")
    if(EXISTS "${_LOCAL_PROTOC}")
        set(Protobuf_PROTOC_EXECUTABLE "${_LOCAL_PROTOC}")
        if(NOT TARGET protobuf::protoc)
            add_executable(protobuf::protoc IMPORTED)
            set_target_properties(protobuf::protoc PROPERTIES IMPORTED_LOCATION "${Protobuf_PROTOC_EXECUTABLE}")
        endif()
    elseif(EXISTS "${_LOCAL_PROTOC_WIN}")
        set(Protobuf_PROTOC_EXECUTABLE "${_LOCAL_PROTOC_WIN}")
        if(NOT TARGET protobuf::protoc)
            add_executable(protobuf::protoc IMPORTED)
            set_target_properties(protobuf::protoc PROPERTIES IMPORTED_LOCATION "${Protobuf_PROTOC_EXECUTABLE}")
        endif()
    endif()
endif()

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

set(SERVER_SRC
    src/main.cpp
    src/server.cpp
    src/config.cpp
    src/net/listener.cpp
    src/net/session.cpp
    src/net/tls_context.cpp
    src/net/directory_client.cpp
    src/db/database.cpp
    src/db/user_store.cpp
    src/db/offline_store.cpp
    src/db/file_store.cpp
    src/channel/channel_manager.cpp
    src/commands/command_handler.cpp
    src/crypto/file_encryptor.cpp
    src/security/virus_scanner.cpp
    src/voice/voice_room_manager.cpp
    src/admin/reserved_identity.cpp
    src/admin/server_owner.cpp
    ${PROTO_SRCS}
)

set(SERVER_HDRS
    src/utils/string_utils.hpp
    src/server.hpp
    src/config.hpp
    src/admin/reserved_identity.hpp
    src/admin/server_owner.hpp
    src/net/listener.hpp
    src/net/session.hpp
    src/net/tls_context.hpp
    src/net/rate_limiter.hpp
    src/net/directory_client.hpp
    src/db/database.hpp
    src/db/user_store.hpp
    src/db/offline_store.hpp
    src/db/file_store.hpp
    src/channel/channel_manager.hpp
    src/commands/command_handler.hpp
    src/crypto/file_encryptor.hpp
    src/security/virus_scanner.hpp
    src/voice/voice_room_manager.hpp
    ${PROTO_HDRS}
)

add_executable(grotto-server ${SERVER_SRC} ${SERVER_HDRS})

target_include_directories(grotto-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(grotto-server PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    spdlog::spdlog
    unofficial-sodium::sodium
    SQLiteCpp
    toml11::toml11
)

if(GROTTO_CHAT_ENABLE_HTTP_API)
    target_link_libraries(grotto-server PRIVATE grotto-server-api)
endif()

# ──────────────────────────────────────────────
# TUI Support (optional)
# ──────────────────────────────────────────────
option(GROTTO_CHAT_SERVER_TUI "Build with TUI support" ON)

if(GROTTO_CHAT_SERVER_TUI)
    # Monorepo workflow: prefer local TUI project and avoid stale fetches to the old split repo.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../grotto-chat-server-tui/CMakeLists.txt")
        message(STATUS "TUI support: Enabled (using local grotto-chat-server-tui)")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../grotto-chat-server-tui ${CMAKE_BINARY_DIR}/grotto-chat-server-tui)
        target_link_libraries(grotto-server PRIVATE grotto-server-tui)
        target_compile_definitions(grotto-server PRIVATE GROTTO_CHAT_HAS_TUI)
    else()
        message(WARNING "TUI support requested, but local grotto-chat-server-tui was not found. Building server without TUI support.")
    endif()
else()
    message(STATUS "TUI support: Disabled (enable with -DGROTTO_CHAT_SERVER_TUI=ON)")
endif()

if(WIN32)
    if(TARGET absl::abseil_dll)
        target_link_libraries(grotto-server PRIVATE absl::abseil_dll)
    endif()

    target_link_libraries(grotto-server PRIVATE ws2_32 crypt32)
    set_target_properties(grotto-server PROPERTIES WIN32_EXECUTABLE FALSE)
else()
    target_link_libraries(grotto-server PRIVATE pthread rt)
endif()

if(MSVC)
    target_compile_options(grotto-server PRIVATE /W4 /WX-)
else()
    target_compile_options(grotto-server PRIVATE -Wall -Wextra -Wpedantic)
endif()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/certs)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/server.toml.example
    ${CMAKE_BINARY_DIR}/server.toml
    COPYONLY
)

add_custom_command(TARGET grotto-server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "$<TARGET_FILE_DIR:grotto-server>/README.md"
    COMMENT "Copying README.md to server output directory"
)

install(TARGETS grotto-server RUNTIME DESTINATION bin)

string(TOLOWER "${CMAKE_SYSTEM_NAME}" GROTTO_CHAT_SERVER_RELEASE_SYSTEM)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" GROTTO_CHAT_SERVER_RELEASE_PROCESSOR)

if(GROTTO_CHAT_SERVER_RELEASE_SYSTEM STREQUAL "linux")
    if(GROTTO_CHAT_SERVER_RELEASE_PROCESSOR MATCHES "^(x86_64|amd64)$")
        set(GROTTO_CHAT_SERVER_RELEASE_PLATFORM "linux-x64")
    elseif(GROTTO_CHAT_SERVER_RELEASE_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(GROTTO_CHAT_SERVER_RELEASE_PLATFORM "linux-arm64")
    else()
        set(GROTTO_CHAT_SERVER_RELEASE_PLATFORM "linux-${GROTTO_CHAT_SERVER_RELEASE_PROCESSOR}")
    endif()
else()
    set(GROTTO_CHAT_SERVER_RELEASE_PLATFORM "${GROTTO_CHAT_SERVER_RELEASE_SYSTEM}-${GROTTO_CHAT_SERVER_RELEASE_PROCESSOR}")
endif()

set(GROTTO_CHAT_SERVER_RELEASE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/release" CACHE PATH "Directory where server release artifacts are generated")
set(GROTTO_CHAT_SERVER_RELEASE_ARCHIVE "grotto-server-${GROTTO_CHAT_SERVER_RELEASE_PLATFORM}.tar.gz" CACHE STRING "Server release archive name")
set(GROTTO_CHAT_SERVER_RELEASE_SHA "${GROTTO_CHAT_SERVER_RELEASE_ARCHIVE}.sha256" CACHE STRING "Server release checksum file name")

add_custom_command(
    OUTPUT
        "${GROTTO_CHAT_SERVER_RELEASE_OUTPUT_DIR}/${GROTTO_CHAT_SERVER_RELEASE_ARCHIVE}"
        "${GROTTO_CHAT_SERVER_RELEASE_OUTPUT_DIR}/${GROTTO_CHAT_SERVER_RELEASE_SHA}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GROTTO_CHAT_SERVER_RELEASE_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E tar "cfz" "${GROTTO_CHAT_SERVER_RELEASE_OUTPUT_DIR}/${GROTTO_CHAT_SERVER_RELEASE_ARCHIVE}" --format=gnutar
        "$<TARGET_FILE_NAME:grotto-server>"
        README.md
        server.toml
    COMMAND ${CMAKE_COMMAND} -E sha256sum "${GROTTO_CHAT_SERVER_RELEASE_OUTPUT_DIR}/${GROTTO_CHAT_SERVER_RELEASE_ARCHIVE}" > "${GROTTO_CHAT_SERVER_RELEASE_OUTPUT_DIR}/${GROTTO_CHAT_SERVER_RELEASE_SHA}"
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:grotto-server>"
    DEPENDS grotto-server
    COMMENT "Creating server release archive ${GROTTO_CHAT_SERVER_RELEASE_ARCHIVE}"
    VERBATIM
)

add_custom_target(grotto-server_release
    DEPENDS
        "${GROTTO_CHAT_SERVER_RELEASE_OUTPUT_DIR}/${GROTTO_CHAT_SERVER_RELEASE_ARCHIVE}"
        "${GROTTO_CHAT_SERVER_RELEASE_OUTPUT_DIR}/${GROTTO_CHAT_SERVER_RELEASE_SHA}"
)


enable_testing()

# ──────────────────────────────────────────────
# Tests
# ──────────────────────────────────────────────
find_package(Catch2 3 REQUIRED)

add_executable(test-file-encryptor
    tests/test_file_encryptor.cpp
    src/crypto/file_encryptor.cpp
)
target_include_directories(test-file-encryptor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(test-file-encryptor PRIVATE
    Catch2::Catch2WithMain
    unofficial-sodium::sodium
    OpenSSL::Crypto
    spdlog::spdlog
)
add_test(NAME file-encryptor-tests COMMAND test-file-encryptor)

add_executable(test-reserved-identity
    tests/test_reserved_identity.cpp
    src/admin/reserved_identity.cpp
)
target_include_directories(test-reserved-identity PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(test-reserved-identity PRIVATE
    Catch2::Catch2WithMain
    spdlog::spdlog
)
add_test(NAME reserved-identity-tests COMMAND test-reserved-identity)

add_executable(test-channel-utils
    tests/test_channel_utils.cpp
)
target_include_directories(test-channel-utils PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(test-channel-utils PRIVATE
    Catch2::Catch2WithMain
)
add_test(NAME channel-utils-tests COMMAND test-channel-utils)
