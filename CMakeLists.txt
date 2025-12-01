cmake_minimum_required(VERSION 3.15)
project(McUpdaterServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/MT$<$<CONFIG:Debug>:d>)
endif()

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/Source/src)
include_directories(Source/include)

include_directories(${INCLUDE_DIR})

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows-static")

find_package(crow REQUIRED)
find_package(Asio REQUIRED)

if(NOT CURL_FOUND)
    find_package(CURL REQUIRED)
endif()

find_package(OpenSSL REQUIRED)

# 修改JsonCpp的查找方式
find_package(jsoncpp REQUIRED)
if(TARGET jsoncpp_lib)
    set(JSONCPP_LIBRARIES jsoncpp_lib)
else()
    # 尝试其他常见的目标名
    find_library(JSONCPP_LIBRARY NAMES jsoncpp)
    if(JSONCPP_LIBRARY)
        set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY})
        find_path(JSONCPP_INCLUDE_DIR json/json.h)
        if(JSONCPP_INCLUDE_DIR)
            include_directories(${JSONCPP_INCLUDE_DIR})
        endif()
    else()
        message(FATAL_ERROR "JsonCpp not found")
    endif()
endif()

if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBZIP libzip)
endif()

if(NOT LIBZIP_FOUND)
    find_path(LIBZIP_INCLUDE_DIR zip.h)
    find_library(LIBZIP_LIBRARY NAMES zip)
    if(LIBZIP_INCLUDE_DIR AND LIBZIP_LIBRARY)
        set(LIBZIP_FOUND TRUE)
        set(LIBZIP_INCLUDE_DIRS ${LIBZIP_INCLUDE_DIR})
        set(LIBZIP_LIBRARIES ${LIBZIP_LIBRARY})
    else()
        message(FATAL_ERROR "libzip not found")
    endif()
endif()

find_package(BZip2 REQUIRED)
if(BZIP2_FOUND)
    include_directories(${BZIP2_INCLUDE_DIRS})
endif()

include_directories(
    ${CROW_INCLUDE_DIRS}
    ${ASIO_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${LIBZIP_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}  # 添加JsonCpp包含目录
)

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4)
    add_compile_options(/wd4996 /wd4267 /wd4244)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT")
endif()

if(UNIX AND NOT APPLE)
    add_compile_options(-Wall -Wextra)
elseif(APPLE)
    add_compile_options(-Wall -Wextra -Wno-deprecated-declarations)
endif()

add_executable(McUpdaterServer
    ${SOURCE_DIR}/main.cpp
    ${SOURCE_DIR}/ZipManager.cpp
    ${SOURCE_DIR}/UpdateApiGenerator.cpp
    ${SOURCE_DIR}/ConfigManager.cpp
    ${SOURCE_DIR}/FileScanner.cpp
    ${SOURCE_DIR}/FileHasher.cpp
    ${SOURCE_DIR}/logger.cpp
)

target_link_libraries(McUpdaterServer
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${JSONCPP_LIBRARIES}  # 链接JsonCpp库
    ${LIBZIP_LIBRARIES}
    ${BZIP2_LIBRARIES}
    ${CROW_LIBRARIES}
    ${ASIO_LIBRARIES}
)

if(WIN32)
    target_link_libraries(McUpdaterServer
        ws2_32
        crypt32
        advapi32
        bcrypt
        iphlpapi
        normaliz
        wldap32
    )
endif()

if(APPLE)
    find_library(CORE_FOUNDATION CoreFoundation)
    find_library(SECURITY Security)
    target_link_libraries(McUpdaterServer 
        ${CORE_FOUNDATION} 
        ${SECURITY}
    )
endif()

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/config)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/logs)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/cache)  # 添加缓存目录

add_custom_command(TARGET McUpdaterServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory 
    ${CMAKE_SOURCE_DIR}/config
    COMMAND ${CMAKE_COMMAND} -E make_directory 
    ${CMAKE_SOURCE_DIR}/logs
    COMMAND ${CMAKE_COMMAND} -E make_directory 
    ${CMAKE_SOURCE_DIR}/cache
    COMMENT "Creating config, logs and cache directories"
)