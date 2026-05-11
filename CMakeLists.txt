cmake_minimum_required(VERSION 3.23)
project(CustomLibraries
    VERSION 1.0.0
    DESCRIPTION "High-performance C++ component library"
    LANGUAGES C CXX
)

# CMake 4.0+ 将 CMP0167 行为改为 NEW（FindBoost 弃用）
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 构建选项
option(CUSTOM_LIBRARIES_BUILD_TESTS "Build unit tests" ON)
option(CUSTOM_LIBRARIES_BUILD_EXAMPLES "Build examples" ON)

# === 所有依赖均通过 FetchContent 从云端拉取 ===
include(FetchContent)

# --- Boost 1.89.0 ---
# 从 Boost 官方归档下载完整源码包（header-only 使用）
FetchContent_Declare(
    Boost
    URL https://archives.boost.io/release/1.89.0/source/boost_1_89_0.tar.bz2
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(Boost)

# --- BoringSSL (OpenSSL 兼容) ---
# Google 维护的 TLS 库，API 兼容 OpenSSL
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BORINGSSL_BUILD_TOOL OFF CACHE BOOL "" FORCE)
set(OPENSSL_NO_ASM ON CACHE BOOL "" FORCE)
find_package(Threads REQUIRED)

FetchContent_Declare(
    boringssl
    URL https://github.com/google/boringssl/archive/beafe3db15e3213bbce1711f85f704b6c00413d1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# CMP0169: 允许在 patch 场景下使用 FetchContent_Populate
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Populate(boringssl)

# MinGW patch: 移除不支持 PE/COFF 格式的汇编文件
if(MINGW)
    set(SOURCES_CMAKE "${boringssl_SOURCE_DIR}/gen/sources.cmake")
    file(READ ${SOURCES_CMAKE} SOURCES_CONTENT)
    string(REPLACE "  third_party/fiat/asm/fiat_p256_adx_mul.S\n" "" SOURCES_CONTENT "${SOURCES_CONTENT}")
    string(REPLACE "  third_party/fiat/asm/fiat_p256_adx_sqr.S\n" "" SOURCES_CONTENT "${SOURCES_CONTENT}")
    file(WRITE ${SOURCES_CMAKE} "${SOURCES_CONTENT}")

    set(P256_64_H "${boringssl_SOURCE_DIR}/third_party/fiat/p256_64.h")
    file(READ ${P256_64_H} P256_CONTENT)
    string(REPLACE
        "#if !defined(OPENSSL_NO_ASM) && defined(__GNUC__) && defined(__x86_64__)"
        "#if !defined(OPENSSL_NO_ASM) && defined(__GNUC__) && defined(__x86_64__) && !defined(OPENSSL_WINDOWS)"
        P256_CONTENT "${P256_CONTENT}")
    file(WRITE ${P256_64_H} "${P256_CONTENT}")
endif()

add_subdirectory(${boringssl_SOURCE_DIR} ${boringssl_BINARY_DIR})

# 创建 OpenSSL 兼容别名
if(NOT TARGET OpenSSL::SSL AND TARGET ssl)
    add_library(OpenSSL::SSL ALIAS ssl)
endif()
if(NOT TARGET OpenSSL::Crypto AND TARGET crypto)
    add_library(OpenSSL::Crypto ALIAS crypto)
endif()

# 排除不需要的 BoringSSL 目标
set_target_properties(bssl PROPERTIES EXCLUDE_FROM_ALL TRUE)
if(TARGET decrepit)
    set_target_properties(decrepit PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

# --- jsoncpp ---
FetchContent_Declare(
    jsoncpp
    URL https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(jsoncpp)

# --- Crypto++ ---
# 加密库，提供 AES/RSA/SHA/HMAC 等算法
FetchContent_Declare(
    cryptopp_src
    URL https://github.com/weidai11/cryptopp/archive/refs/tags/CRYPTOPP_8_9_0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(cryptopp_src)

# 收集所有 cryptopp 源文件（排除 test 和 bench 程序）
file(GLOB CRYPTOPP_SOURCES "${cryptopp_src_SOURCE_DIR}/*.cpp")
list(FILTER CRYPTOPP_SOURCES EXCLUDE REGEX ".*test\\.cpp$")
list(FILTER CRYPTOPP_SOURCES EXCLUDE REGEX ".*bench\\.cpp$")
list(FILTER CRYPTOPP_SOURCES EXCLUDE REGEX ".*validat[0-9]*\\.cpp$")
list(FILTER CRYPTOPP_SOURCES EXCLUDE REGEX ".*datatest\\.cpp$")
list(FILTER CRYPTOPP_SOURCES EXCLUDE REGEX ".*regtest\\.cpp$")
list(FILTER CRYPTOPP_SOURCES EXCLUDE REGEX ".*fipstest\\.cpp$")

add_library(cryptopp STATIC ${CRYPTOPP_SOURCES})

target_include_directories(cryptopp PUBLIC
    $<BUILD_INTERFACE:${cryptopp_src_SOURCE_DIR}>
)

set_target_properties(cryptopp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 抑制 cryptopp 源码中的编译器警告
if(MSVC)
    target_compile_options(cryptopp PRIVATE /W0)
else()
    target_compile_options(cryptopp PRIVATE -w)
    target_compile_definitions(cryptopp PRIVATE CRYPTOPP_DISABLE_ASM)
endif()

# --- Header-only 库目标 ---
add_library(custom_libraries INTERFACE)
add_library(CustomLibraries::CustomLibraries ALIAS custom_libraries)

target_include_directories(custom_libraries INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(custom_libraries INTERFACE cxx_std_20)

# Boost header-only（直接添加 include 路径 + 宏定义）
target_include_directories(custom_libraries INTERFACE ${boost_SOURCE_DIR})
target_compile_definitions(custom_libraries INTERFACE
    WIN32_LEAN_AND_MEAN
    BOOST_ERROR_CODE_HEADER_ONLY
    BOOST_SYSTEM_NO_LIB
    BOOST_URL_NO_LIB
    BOOST_LOG_NO_LIB
    BOOST_REGEX_NO_LIB
    BOOST_FILESYSTEM_NO_LIB
    BOOST_THREAD_NO_LIB
    BOOST_CHRONO_NO_LIB
    BOOST_TIMER_NO_LIB
    BOOST_ATOMIC_NO_LIB
    BOOST_PROGRAM_OPTIONS_NO_LIB
    BOOST_CONTAINER_NO_LIB
    BOOST_DATE_TIME_NO_LIB
    BOOST_COROUTINES_NO_LIB
    BOOST_FIBER_NO_LIB
    BOOST_CONTEXT_NO_LIB
    BOOST_ASIO_HEADER_ONLY
    BOOST_ASIO_USE_OPENSSL
    BOOST_JSON_NO_LIB
)

target_link_libraries(custom_libraries INTERFACE
    OpenSSL::SSL
    OpenSSL::Crypto
    jsoncpp_lib
    cryptopp
    custom_libraries_boost_json_impl
)

# Boost.JSON 实现（在单个翻译单元中编译 boost/json/src.hpp）
add_library(custom_libraries_boost_json_impl STATIC
    wan/network/agreement/json.cpp
)
target_include_directories(custom_libraries_boost_json_impl PUBLIC ${boost_SOURCE_DIR})
target_compile_definitions(custom_libraries_boost_json_impl PUBLIC
    WIN32_LEAN_AND_MEAN
    BOOST_ERROR_CODE_HEADER_ONLY
    BOOST_SYSTEM_NO_LIB
    BOOST_ASIO_HEADER_ONLY
)
target_compile_options(custom_libraries_boost_json_impl PRIVATE -w)
target_compile_features(custom_libraries_boost_json_impl PUBLIC cxx_std_20)

# 平台特定库
if(WIN32)
    target_link_libraries(custom_libraries INTERFACE
        ws2_32
        mswsock
        crypt32
        advapi32
        bcrypt
    )
endif()

# 子目录
if(CUSTOM_LIBRARIES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CUSTOM_LIBRARIES_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 安装规则（如需安装到系统，取消以下注释）
# include(GNUInstallDirs)
# include(CMakePackageConfigHelpers)
# install(TARGETS custom_libraries EXPORT CustomLibrariesTargets)
# install(DIRECTORY model/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")
# install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")
# install(EXPORT CustomLibrariesTargets FILE CustomLibrariesTargets.cmake
#     NAMESPACE CustomLibraries:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CustomLibraries)
