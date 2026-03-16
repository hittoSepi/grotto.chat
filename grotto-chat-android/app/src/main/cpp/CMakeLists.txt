cmake_minimum_required(VERSION 3.22.1)
project(grotto-native)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android NDK already provides OpenSSL (libcrypto)
# We need to fetch libsignal-protocol-c and use libsodium

include(FetchContent)

# Fetch libsignal-protocol-c
FetchContent_Declare(
    libsignal-protocol-c
    GIT_REPOSITORY https://github.com/signalapp/libsignal-protocol-c.git
    GIT_TAG v2.3.3
    GIT_SHALLOW TRUE
)




# Configure libsignal-protocol-c build for Android
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libsignal-protocol-c)

# NOTE: libsodium is not used - using OpenSSL from NDK instead

# Native crypto library
add_library(grotto-native SHARED
    jni/jni_bridge.cpp
    jni/jni_voice_bridge.cpp
    crypto/crypto_engine.cpp
    crypto/identity.cpp
    crypto/signal_store.cpp
    crypto/group_session.cpp
    voice/voice_engine.cpp
)



# Find OpenSSL headers from NDK
# Android NDK provides OpenSSL/BoringSSL headers in the sysroot
if(ANDROID_NDK)
    # Try different possible locations for NDK headers
    set(NDK_OPENSSL_INCLUDE "")
    
    # Option 1: Standard LLVM toolchain path
    file(GLOB NDK_LLVM_PATHS "${ANDROID_NDK}/toolchains/llvm/prebuilt/*/sysroot/usr/include")
    if(NDK_LLVM_PATHS)
        list(GET NDK_LLVM_PATHS 0 NDK_OPENSSL_INCLUDE)
    endif()
    
    # Option 2: Alternative paths
    if(NOT NDK_OPENSSL_INCLUDE OR NOT EXISTS "${NDK_OPENSSL_INCLUDE}/openssl/evp.h")
        if(EXISTS "${ANDROID_NDK}/sysroot/usr/include/openssl/evp.h")
            set(NDK_OPENSSL_INCLUDE "${ANDROID_NDK}/sysroot/usr/include")
        elseif(EXISTS "${ANDROID_NDK}/usr/include/openssl/evp.h")
            set(NDK_OPENSSL_INCLUDE "${ANDROID_NDK}/usr/include")
        endif()
    endif()
    
    # Option 3: Use CMake's find_path as fallback
    if(NOT NDK_OPENSSL_INCLUDE OR NOT EXISTS "${NDK_OPENSSL_INCLUDE}/openssl/evp.h")
        find_path(NDK_OPENSSL_INCLUDE 
            NAMES openssl/evp.h
            PATHS 
                "${ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/sysroot/usr/include"
                "${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include"
                "${ANDROID_NDK}/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/include"
                "${ANDROID_NDK}/sysroot/usr/include"
            NO_DEFAULT_PATH
        )
    endif()
    
    if(NDK_OPENSSL_INCLUDE AND EXISTS "${NDK_OPENSSL_INCLUDE}/openssl/evp.h")
        message(STATUS "Found OpenSSL headers at: ${NDK_OPENSSL_INCLUDE}")
    else()
        message(WARNING "OpenSSL headers not found in NDK. Build may fail.")
        set(NDK_OPENSSL_INCLUDE "")
    endif()
else()
    message(WARNING "ANDROID_NDK not set. OpenSSL headers may not be found.")
    set(NDK_OPENSSL_INCLUDE "")
endif()

target_include_directories(grotto-native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${libsignal-protocol-c_SOURCE_DIR}/src
    "C:/Users/hitto/Desktop/Grotto-0.15/grotto-android/app/include"
    ${NDK_OPENSSL_INCLUDE}
)

# Prebuilt OpenSSL libraries
set(OPENSSL_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../lib")
get_filename_component(OPENSSL_LIB_DIR "${OPENSSL_LIB_DIR}" ABSOLUTE)
message(STATUS "OpenSSL lib dir: ${OPENSSL_LIB_DIR}")

add_library(openssl_crypto SHARED IMPORTED)
set_target_properties(openssl_crypto PROPERTIES IMPORTED_LOCATION "${OPENSSL_LIB_DIR}/libcrypto.so")

add_library(openssl_ssl SHARED IMPORTED)
set_target_properties(openssl_ssl PROPERTIES IMPORTED_LOCATION "${OPENSSL_LIB_DIR}/libssl.so")

# Link libraries
target_link_libraries(grotto-native
    signal-protocol-c
    openssl_crypto
    openssl_ssl
    log         # Android log
)

# Compiler flags for Android
target_compile_options(grotto-native PRIVATE
    -Wall
    -Wextra
    -fexceptions
    -frtti
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(grotto-native PRIVATE -g -O0)
else()
    target_compile_options(grotto-native PRIVATE -O3 -fvisibility=hidden -flto)
    target_link_options(grotto-native PRIVATE -flto)
endif()
