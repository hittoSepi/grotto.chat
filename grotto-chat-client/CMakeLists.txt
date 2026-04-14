cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

set(PROJECT_NAME "grotto-client")
set(PROJECT_DESCRIPTION "Grotto Desktop and Linux application")

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

project( ${PROJECT_NAME} VERSION ${PROJECT_VERSION} DESCRIPTION ${PROJECT_DESCRIPTION} LANGUAGES C CXX )

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Export compile commands for tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
add_compile_definitions(BOOST_ALL_NO_LIB)
endif()



# ──────────────────────────────────────────────
# vcpkg dependencies
# ──────────────────────────────────────────────
set(Boost_NO_BOOST_CMAKE ON)
cmake_policy(SET CMP0074 NEW)
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(absl CONFIG REQUIRED)
find_package(SQLiteCpp REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(LibDataChannel CONFIG REQUIRED)
find_package(Opus REQUIRED)
find_package(CURL REQUIRED)
find_package(toml11 CONFIG REQUIRED)
find_package(spdlog REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)

# ──────────────────────────────────────────────
# FetchContent: libsignal-protocol-c
# ──────────────────────────────────────────────
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(
    signal_protocol_c
    GIT_REPOSITORY https://github.com/signalapp/libsignal-protocol-c.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# Use low-level populate so we can patch before add_subdirectory
FetchContent_GetProperties(signal_protocol_c)
if(NOT signal_protocol_c_POPULATED)
    FetchContent_Populate(signal_protocol_c)

    # Patch 1: remove internal_fast_tests.c from curve25519 CMakeLists to fix
    # MSVC VLA compile error in that test file.
    set(_curve_cmake "${signal_protocol_c_SOURCE_DIR}/src/curve25519/CMakeLists.txt")
    if(EXISTS "${_curve_cmake}")
        file(READ "${_curve_cmake}" _curve_content)
        string(REPLACE "ed25519/tests/internal_fast_tests.c" "" _curve_content "${_curve_content}")
        string(REPLACE "internal_fast_tests.c" "" _curve_content "${_curve_content}")
        file(WRITE "${_curve_cmake}" "${_curve_content}")
    endif()

    # Patch 2: stub out curve_internal_fast_tests in curve.c so it no longer
    # calls all_fast_tests (which was defined in the removed test file).
    set(_curve_c "${signal_protocol_c_SOURCE_DIR}/src/curve.c")
    if(EXISTS "${_curve_c}")
        file(READ "${_curve_c}" _curve_c_content)
        string(REPLACE
            "    if (all_fast_tests(silent) != 0)\n        return SG_ERR_UNKNOWN;\n    return 0;"
            "    (void)silent;\n    /* internal_fast_tests.c removed (MSVC VLA incompatible) */\n    return 0;"
            _curve_c_content "${_curve_c_content}")
        file(WRITE "${_curve_c}" "${_curve_c_content}")
    endif()

    add_subdirectory("${signal_protocol_c_SOURCE_DIR}" "${signal_protocol_c_BINARY_DIR}")
endif()

# ──────────────────────────────────────────────
# FetchContent: miniaudio (header-only)
# ──────────────────────────────────────────────
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(miniaudio)

FetchContent_Declare(
    rnnoise
    GIT_REPOSITORY https://github.com/xiph/rnnoise.git
    GIT_TAG        904a876dce1f9ab8860c0a5000ed151f9f6eef58 # v0.2
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(rnnoise)
if(NOT rnnoise_POPULATED)
    FetchContent_Populate(rnnoise)
endif()

set(GROTTO_RNNOISE_MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise-model/src")
set(GROTTO_RNNOISE_COMPAT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise-compat")
set(_RNNOISE_MODEL_C "${GROTTO_RNNOISE_MODEL_DIR}/rnnoise_data.c")
set(_RNNOISE_MODEL_H "${GROTTO_RNNOISE_MODEL_DIR}/rnnoise_data.h")

if(NOT EXISTS "${_RNNOISE_MODEL_C}" OR NOT EXISTS "${_RNNOISE_MODEL_H}")
    set(_RNNOISE_MODEL_VERSION "0b50c45")
    set(_RNNOISE_MODEL_ARCHIVE "${rnnoise_SOURCE_DIR}/rnnoise_data-${_RNNOISE_MODEL_VERSION}.tar.gz")
    set(_RNNOISE_MODEL_URL "https://media.xiph.org/rnnoise/models/rnnoise_data-${_RNNOISE_MODEL_VERSION}.tar.gz")

    if(NOT EXISTS "${rnnoise_SOURCE_DIR}/src/rnnoise_data.c" OR
       NOT EXISTS "${rnnoise_SOURCE_DIR}/src/rnnoise_data.h")
        message(STATUS "Downloading RNNoise default model archive: ${_RNNOISE_MODEL_URL}")
        file(DOWNLOAD
            "${_RNNOISE_MODEL_URL}"
            "${_RNNOISE_MODEL_ARCHIVE}"
            STATUS _rnnoise_download_status
            SHOW_PROGRESS
        )
        list(GET _rnnoise_download_status 0 _rnnoise_download_code)
        list(GET _rnnoise_download_status 1 _rnnoise_download_message)
        if(NOT _rnnoise_download_code EQUAL 0)
            message(FATAL_ERROR
                "Failed to download RNNoise model archive (${_RNNOISE_MODEL_URL}): ${_rnnoise_download_message}")
        endif()

        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf "${_RNNOISE_MODEL_ARCHIVE}"
            WORKING_DIRECTORY "${rnnoise_SOURCE_DIR}"
            RESULT_VARIABLE _rnnoise_extract_result
        )
        if(NOT _rnnoise_extract_result EQUAL 0)
            message(FATAL_ERROR
                "Failed to extract RNNoise model archive: ${_RNNOISE_MODEL_ARCHIVE}")
        endif()
    endif()

    set(_RNNOISE_MODEL_C "${rnnoise_SOURCE_DIR}/src/rnnoise_data.c")
    set(_RNNOISE_MODEL_H "${rnnoise_SOURCE_DIR}/src/rnnoise_data.h")
endif()

add_library(grotto_rnnoise STATIC
    ${rnnoise_SOURCE_DIR}/src/denoise.c
    ${rnnoise_SOURCE_DIR}/src/rnn.c
    ${rnnoise_SOURCE_DIR}/src/pitch.c
    ${rnnoise_SOURCE_DIR}/src/kiss_fft.c
    ${rnnoise_SOURCE_DIR}/src/celt_lpc.c
    ${rnnoise_SOURCE_DIR}/src/nnet.c
    ${rnnoise_SOURCE_DIR}/src/nnet_default.c
    ${rnnoise_SOURCE_DIR}/src/parse_lpcnet_weights.c
    ${_RNNOISE_MODEL_C}
    ${rnnoise_SOURCE_DIR}/src/rnnoise_tables.c
)
target_include_directories(grotto_rnnoise
    PUBLIC
        ${rnnoise_SOURCE_DIR}/include
    PRIVATE
        ${rnnoise_SOURCE_DIR}/src
        ${GROTTO_RNNOISE_MODEL_DIR}
        ${GROTTO_RNNOISE_COMPAT_DIR}
)
target_compile_definitions(grotto_rnnoise PRIVATE RNNOISE_BUILD=1)
if(NOT MSVC)
    target_link_libraries(grotto_rnnoise PUBLIC m)
endif()

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(stb)

if(NOT TARGET miniaudio::miniaudio)
    add_library(miniaudio_hdr INTERFACE)
    target_include_directories(miniaudio_hdr INTERFACE ${miniaudio_SOURCE_DIR})
    add_library(miniaudio::miniaudio ALIAS miniaudio_hdr)
endif()

# ──────────────────────────────────────────────
# Protobuf code generation
# ──────────────────────────────────────────────
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/grotto.proto")
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

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

# ──────────────────────────────────────────────
# Source files
# ──────────────────────────────────────────────
set(CLIENT_SOURCES
    src/main.cpp
    src/app.cpp
    src/connection_status.cpp
    src/config.cpp
    src/runtime/runtime_capabilities.cpp
    src/state/app_state.cpp
    src/ui/ui_manager.cpp
    src/ui/login_screen.cpp
    src/ui/settings_screen.cpp
    src/ui/settings_layout.cpp
    src/ui/settings_categories_general.cpp
    src/ui/settings_categories_voice.cpp
    src/ui/settings_categories_misc.cpp
    src/ui/message_view.cpp
    src/ui/graphics_compositor.cpp
    src/ui/input_line.cpp
    src/ui/tab_bar.cpp
    src/ui/status_bar.cpp
    src/ui/graphics_layout.cpp
    src/ui/link_preview.cpp
    src/ui/terminal_image.cpp
    src/ui/files_panel.cpp
    src/ui/user_list_panel.cpp
    src/ui/markdown_renderer.cpp
    src/ui/mouse_support.cpp
    src/net/net_client.cpp
    src/net/message_handler.cpp
    src/crypto/crypto_engine.cpp
    src/crypto/identity.cpp
    src/crypto/signal_store.cpp
    src/crypto/group_session.cpp
    src/voice/voice_engine.cpp
    src/voice/audio_device.cpp
    src/voice/opus_codec.cpp
    src/voice/jitter_buffer.cpp
    src/voice/limiter.cpp
    src/voice/noise_suppressor.cpp
    src/preview/link_previewer.cpp
    src/db/local_store.cpp
    src/db/migrations.cpp
    src/input/input_handler.cpp
    src/input/command_parser.cpp
    src/input/tab_complete.cpp
    src/help/help_manager.cpp
    src/i18n/strings.cpp
    src/file/file_transfer.cpp
    ${PROTO_SRCS}
)

add_executable(grotto-client ${CLIENT_SOURCES})

target_include_directories(grotto-client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${signal_protocol_c_SOURCE_DIR}/src
    ${stb_SOURCE_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(grotto-client PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    SQLiteCpp
    unofficial-sodium::sodium
    LibDataChannel::LibDataChannel
    Opus::opus
    CURL::libcurl
    toml11::toml11
    spdlog::spdlog
    signal-protocol-c
    miniaudio::miniaudio
    grotto_rnnoise
    ftxui::screen
    ftxui::dom
    ftxui::component
  
)
target_compile_definitions(grotto-client PRIVATE GROTTO_HAS_RNNOISE=1)

if(MSVC)
    target_compile_options(grotto-client PRIVATE /W3 /wd4996)
else()
    target_compile_options(grotto-client PRIVATE -Wall -Wextra)
endif()

# Copy README to build output directory
add_custom_command(TARGET grotto-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "$<TARGET_FILE_DIR:grotto-client>/README.md"
    COMMENT "Copying README.md to output directory"
)

# Copy client config to build output directory
add_custom_command(TARGET grotto-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/client.toml.example"
        "$<TARGET_FILE_DIR:grotto-client>/client.toml"
    COMMENT "Copying client.toml to output directory"
)

# Copy help files to build output directory
add_custom_command(TARGET grotto-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/help/en"
        "$<TARGET_FILE_DIR:grotto-client>/help/en"
    COMMENT "Copying help/en/ to output directory"
)
add_custom_command(TARGET grotto-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/help/fi"
        "$<TARGET_FILE_DIR:grotto-client>/help/fi"
    COMMENT "Copying help/fi/ to output directory"
)
add_custom_command(TARGET grotto-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:grotto-client>/resources"
    COMMENT "Copying resources/ to output directory"
)


enable_testing()

add_executable(test-input-line
    tests/test_input_line.cpp
    src/ui/input_line.cpp
)
target_include_directories(test-input-line PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test-input-line PRIVATE Catch2::Catch2WithMain)
add_test(NAME input-line-tests COMMAND test-input-line)

add_executable(test-config
    tests/test_config.cpp
    src/config.cpp
    src/i18n/strings.cpp
)
target_include_directories(test-config PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test-config PRIVATE
    Catch2::Catch2WithMain
    toml11::toml11
    spdlog::spdlog
)
add_test(NAME config-tests COMMAND test-config)

add_executable(test-voice-helpers
    tests/test_voice_helpers.cpp
)
target_include_directories(test-voice-helpers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test-voice-helpers PRIVATE Catch2::Catch2WithMain)
add_test(NAME voice-helper-tests COMMAND test-voice-helpers)

add_executable(test-noise-suppressor
    tests/test_noise_suppressor.cpp
    src/voice/noise_suppressor.cpp
)
target_include_directories(test-noise-suppressor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(test-noise-suppressor PRIVATE GROTTO_HAS_RNNOISE=1)
target_link_libraries(test-noise-suppressor PRIVATE
    Catch2::Catch2WithMain
    spdlog::spdlog
    grotto_rnnoise
)
add_test(NAME noise-suppressor-tests COMMAND test-noise-suppressor)

add_executable(test-signal-prekey-roundtrip
    tests/test_signal_prekey_roundtrip.cpp
    src/crypto/crypto_engine.cpp
    src/crypto/identity.cpp
    src/crypto/signal_store.cpp
    src/crypto/group_session.cpp
    src/db/local_store.cpp
    src/db/migrations.cpp
    ${PROTO_SRCS}
)
target_include_directories(test-signal-prekey-roundtrip PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${signal_protocol_c_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
)
target_link_libraries(test-signal-prekey-roundtrip PRIVATE
    Catch2::Catch2WithMain
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
    SQLiteCpp
    unofficial-sodium::sodium
    spdlog::spdlog
    signal-protocol-c
    miniaudio::miniaudio
)
add_test(NAME signal-prekey-roundtrip-tests COMMAND test-signal-prekey-roundtrip)

add_executable(test-file-transfer
    tests/test_file_transfer.cpp
    src/file/file_transfer.cpp
    ${PROTO_SRCS}
)
target_include_directories(test-file-transfer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(test-file-transfer PRIVATE
    Catch2::Catch2WithMain
    protobuf::libprotobuf
    unofficial-sodium::sodium
    spdlog::spdlog
)
add_test(NAME file-transfer-tests COMMAND test-file-transfer)

add_executable(test-command-parser
    tests/test_command_parser.cpp
    src/input/command_parser.cpp
)
target_include_directories(test-command-parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test-command-parser PRIVATE
    Catch2::Catch2WithMain
)
add_test(NAME command-parser-tests COMMAND test-command-parser)

add_executable(test-connection-status
    tests/test_connection_status.cpp
    src/connection_status.cpp
    src/i18n/strings.cpp
)
target_include_directories(test-connection-status PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test-connection-status PRIVATE
    Catch2::Catch2WithMain
)
add_test(NAME connection-status-tests COMMAND test-connection-status)

set(GROTTO_CLIENT_TEST_TARGETS
    test-input-line
    test-config
    test-voice-helpers
    test-noise-suppressor
    test-signal-prekey-roundtrip
    test-file-transfer
    test-command-parser
    test-connection-status
)

if(CMAKE_CONFIGURATION_TYPES)
    set(GROTTO_CLIENT_CTEST_ARGS -C $<CONFIG>)
else()
    set(GROTTO_CLIENT_CTEST_ARGS)
endif()

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} ${GROTTO_CLIENT_CTEST_ARGS} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${GROTTO_CLIENT_TEST_TARGETS}
    USES_TERMINAL
    COMMAND_EXPAND_LISTS
    COMMENT "Build and run grotto-chat-client tests"
)

if(WIN32)
    add_custom_target(qa-error-scenarios
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-error-qa.ps1"
            -BuildDir "${CMAKE_BINARY_DIR}"
            -SkipBuild
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS grotto-client check
        USES_TERMINAL
        COMMENT "Prepare repeatable client error-scenario QA workspace"
    )
else()
    add_custom_target(qa-error-scenarios
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-error-qa.sh"
            "${CMAKE_BINARY_DIR}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS grotto-client check
        USES_TERMINAL
        COMMENT "Prepare repeatable client error-scenario QA workspace"
    )
endif()

