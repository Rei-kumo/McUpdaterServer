cmake_minimum_required(VERSION 3.15)
project(McUpdaterServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_compile_definitions(UNICODE_UNICODE)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/MT$<$<CONFIG:Debug>:d>)
    add_compile_options(
        /source-charset:utf-8
        /execution-charset:utf-8)
endif()

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/Source/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Source/include)

include_directories(${INCLUDE_DIR})

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows-static")

find_package(crow REQUIRED)
find_package(Asio REQUIRED)

if(NOT CURL_FOUND)
    find_package(CURL REQUIRED)
endif()

find_package(OpenSSL REQUIRED)

# 查找JsonCpp
find_package(jsoncpp REQUIRED)
if(TARGET jsoncpp_lib)
    set(JSONCPP_LIBRARIES jsoncpp_lib)
elseif(TARGET jsoncpp_static)
    set(JSONCPP_LIBRARIES jsoncpp_static)
else()
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

# 查找libzip
find_path(LIBZIP_INCLUDE_DIR zip.h)
find_library(LIBZIP_LIBRARY NAMES zip)
if(LIBZIP_INCLUDE_DIR AND LIBZIP_LIBRARY)
    set(LIBZIP_FOUND TRUE)
    set(LIBZIP_INCLUDE_DIRS ${LIBZIP_INCLUDE_DIR})
    set(LIBZIP_LIBRARIES ${LIBZIP_LIBRARY})
else()
    message(FATAL_ERROR "libzip not found")
endif()

find_package(BZip2 REQUIRED)
if(BZIP2_FOUND)
    include_directories(${BZIP2_INCLUDE_DIRS})
endif()

include_directories(
    ${CROW_INCLUDE_DIRS}
    ${ASIO_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${LIBZIP_INCLUDE_DIRS}
    ${BZIP2_INCLUDE_DIRS}
)

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS)
    add_compile_options(/W4 /wd4996 /wd4267 /wd4244 /wd4100)
endif()

# 源文件列表 - 确保包含所有.cpp文件
# 更新源文件列表
# 更新源文件列表
set(SOURCES
    ${SOURCE_DIR}/main.cpp
    
    
    
    
    
    
    
    
    
    
    
    
)

add_executable(McUpdaterServer ${SOURCES}        "Source/include/Language.h" "Source/include/Config.h" "Source/src/Config.cpp" "Source/include/FileScanner.h" "Source/src/FileScanner.cpp" "Source/include/DiffEngine.h" "Source/src/DiffEngine.cpp" "Source/include/PackageBuilder.h" "Source/src/PackageBuilder.cpp" "Source/include/VersionManager.h" "Source/src/VersionManager.cpp" "Source/include/WebServer.h" "Source/src/WebServer.cpp" "Source/include/UpdateGenerator.h" "Source/src/UpdateGenerator.cpp" "Source/src/Language.cpp"   "Source/include/Logger.h" "Source/src/Logger.cpp")

# 链接库
target_link_libraries(McUpdaterServer
    ${CURL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${JSONCPP_LIBRARIES}
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

# 创建必要的目录
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/config)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/logs)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/cache)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/public)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/archives)

add_custom_command(TARGET McUpdaterServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/config
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/logs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/cache
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/public
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/archives
    COMMENT "Creating necessary directories"
)

if(WIN32)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()