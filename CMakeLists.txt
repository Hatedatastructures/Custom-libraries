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

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 构建选项
option(CustomLibrariesBuildTests "Build unit tests" ON)
option(CustomLibrariesBuildExamples "Build examples" ON)
option(CustomLibrariesBuildBenchmarks "Build optional performance benchmarks" OFF)
option(CustomLibrariesEnableSanitizers "Enable AddressSanitizer for project consumers" OFF)
option(CustomLibrariesRequireDoxygen
    "Fail configuration if Doxygen is unavailable" OFF)

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

if(TARGET jsoncpp_lib)
    set(CustomLibrariesJsonCppTarget jsoncpp_lib)
elseif(TARGET jsoncpp_static)
    set(CustomLibrariesJsonCppTarget jsoncpp_static)
else()
    message(FATAL_ERROR "No usable JsonCpp library target was created")
endif()

# 抑制 jsoncpp 源码中的编译器警告（MSVC pragma 和 uninitialized）
foreach(JsonCppTarget IN ITEMS jsoncpp_lib jsoncpp_static jsoncpp_object)
    if(TARGET ${JsonCppTarget})
        get_target_property(JsonCppAliasedTarget ${JsonCppTarget} ALIASED_TARGET)
        if(NOT JsonCppAliasedTarget)
            if(MSVC)
                target_compile_options(${JsonCppTarget} PRIVATE /W0)
            else()
                target_compile_options(${JsonCppTarget} PRIVATE
                    -Wno-unknown-pragmas -Wno-uninitialized)
            endif()
        endif()
    endif()
endforeach()

# --- Crypto++ ---
# 加密库，提供 AES/RSA/SHA/HMAC 等算法
FetchContent_Declare(
    cryptopp_src
    URL https://github.com/weidai11/cryptopp/archive/refs/tags/CRYPTOPP_8_9_0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(cryptopp_src)

if(MSVC)
    include(CMake/PatchCryptoPPMsvcIterators.cmake)
endif()

# 收集所有 cryptopp 源文件（排除 test 和 bench 程序）
file(GLOB CryptoppSources "${cryptopp_src_SOURCE_DIR}/*.cpp")
list(FILTER CryptoppSources EXCLUDE REGEX ".*test\\.cpp$")
list(FILTER CryptoppSources EXCLUDE REGEX ".*bench\\.cpp$")
list(FILTER CryptoppSources EXCLUDE REGEX ".*validat[0-9]*\\.cpp$")
list(FILTER CryptoppSources EXCLUDE REGEX ".*datatest\\.cpp$")
list(FILTER CryptoppSources EXCLUDE REGEX ".*regtest\\.cpp$")
list(FILTER CryptoppSources EXCLUDE REGEX ".*fipstest\\.cpp$")

add_library(cryptopp STATIC ${CryptoppSources})

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
    target_compile_definitions(cryptopp PUBLIC CRYPTOPP_DISABLE_ASM)
endif()

# --- Wan 库目标 ---
# Wan/Web 为新增协程驱动 Web 模块
# Wan/Network 为旧模块（逐步废弃）
# Wan/Sched 为旧线程池模块（逐步废弃）
# Wan/Container 为容器模块
# Wan/Chronicle 为协程日志模块

add_library(CustomLibraries INTERFACE)
add_library(CustomLibraries::CustomLibraries ALIAS CustomLibraries)

target_include_directories(CustomLibraries INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:Include>
)

target_compile_features(CustomLibraries INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(CustomLibraries INTERFACE /bigobj)
endif()

# Boost header-only（直接添加 include 路径 + 宏定义）
target_include_directories(CustomLibraries INTERFACE ${boost_SOURCE_DIR})
target_compile_definitions(CustomLibraries INTERFACE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
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

target_link_libraries(CustomLibraries INTERFACE
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CustomLibrariesJsonCppTarget}
    cryptopp
    CustomLibrariesBoostJsonImpl
    CustomLibrariesBoostUrlImpl
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_path(CustomLibrariesLiburingIncludeDir liburing.h)
    find_library(CustomLibrariesLiburingLibrary NAMES uring)
    if(NOT CustomLibrariesLiburingIncludeDir
        OR NOT CustomLibrariesLiburingLibrary)
        message(FATAL_ERROR
            "Linux builds require liburing development headers and library")
    endif()
    target_include_directories(CustomLibraries INTERFACE
        "${CustomLibrariesLiburingIncludeDir}")
    target_compile_definitions(CustomLibraries INTERFACE
        BOOST_ASIO_HAS_IO_URING)
    target_link_libraries(CustomLibraries INTERFACE
        "${CustomLibrariesLiburingLibrary}")
endif()

if(CustomLibrariesEnableSanitizers)
    if(MSVC)
        target_compile_options(CustomLibraries INTERFACE /fsanitize=address)
        target_link_options(CustomLibraries INTERFACE /fsanitize=address)
    else()
        target_compile_options(CustomLibraries INTERFACE
            -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(CustomLibraries INTERFACE -fsanitize=address)
    endif()
endif()

# Boost.JSON 实现（在单个翻译单元中编译 boost/json/src.hpp）
# 新版: Wan/Json/Json.cpp
add_library(CustomLibrariesBoostJsonImpl STATIC
    Wan/Json/Json.cpp
)
target_include_directories(CustomLibrariesBoostJsonImpl PUBLIC ${boost_SOURCE_DIR})
target_compile_definitions(CustomLibrariesBoostJsonImpl PUBLIC
    WIN32_LEAN_AND_MEAN
    BOOST_ERROR_CODE_HEADER_ONLY
    BOOST_SYSTEM_NO_LIB
    BOOST_ASIO_HEADER_ONLY
)
if(MSVC)
    target_compile_options(CustomLibrariesBoostJsonImpl PRIVATE /W0)
else()
    target_compile_options(CustomLibrariesBoostJsonImpl PRIVATE -w)
endif()
target_compile_features(CustomLibrariesBoostJsonImpl PUBLIC cxx_std_23)

# Boost.URL 实现（Boost 1.89 已取消单翻译单元 src.hpp 集成）
file(GLOB_RECURSE CustomLibrariesBoostUrlSources CONFIGURE_DEPENDS
    "${boost_SOURCE_DIR}/libs/url/src/*.cpp"
)
add_library(CustomLibrariesBoostUrlImpl STATIC
    ${CustomLibrariesBoostUrlSources}
)
target_include_directories(CustomLibrariesBoostUrlImpl PUBLIC ${boost_SOURCE_DIR})
target_compile_definitions(CustomLibrariesBoostUrlImpl PUBLIC
    BOOST_ERROR_CODE_HEADER_ONLY
    BOOST_SYSTEM_NO_LIB
    BOOST_URL_NO_LIB
    PRIVATE BOOST_URL_SOURCE
)
target_compile_features(CustomLibrariesBoostUrlImpl PUBLIC cxx_std_23)

# 平台特定库
if(WIN32)
    target_link_libraries(CustomLibraries INTERFACE
        ws2_32
        mswsock
        crypt32
        advapi32
        bcrypt
    )
endif()

# 子目录
if(CustomLibrariesBuildTests)
    enable_testing()
    add_subdirectory(Tests)
endif()

if(CustomLibrariesBuildExamples)
    add_subdirectory(Examples)
endif()

if(CustomLibrariesBuildBenchmarks)
    add_subdirectory(Benchmarks)
endif()

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Docs/Doxygen/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        @ONLY
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Docs/Doxygen/DoxyfileInternal.in"
        "${CMAKE_CURRENT_BINARY_DIR}/DoxyfileInternal"
        @ONLY
    )
    add_custom_target(CustomLibrariesDoxygen
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/Documentation/Doxygen"
        COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
    )
    add_custom_target(CustomLibrariesDoxygenInternalAudit
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/Documentation/DoxygenInternalAudit"
        COMMAND "${DOXYGEN_EXECUTABLE}"
            "${CMAKE_CURRENT_BINARY_DIR}/DoxyfileInternal"
        COMMAND "${CMAKE_COMMAND}"
            "-DWARNINGS_FILE=${CMAKE_CURRENT_BINARY_DIR}/Documentation/DoxygenInternalAudit/warnings.log"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/Docs/Doxygen/ValidateInternalWarnings.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM
    )
elseif(CustomLibrariesRequireDoxygen)
    message(FATAL_ERROR
        "CustomLibrariesRequireDoxygen is enabled, but Doxygen was not found")
endif()

# 安装规则（如需安装到系统，取消以下注释）
# include(GNUInstallDirs)
# include(CMakePackageConfigHelpers)
# install(TARGETS CustomLibraries EXPORT CustomLibrariesTargets)
# install(DIRECTORY Wan/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")
# install(DIRECTORY Include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.hpp")
# install(EXPORT CustomLibrariesTargets FILE CustomLibrariesTargets.cmake
#     NAMESPACE CustomLibraries:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CustomLibraries)
