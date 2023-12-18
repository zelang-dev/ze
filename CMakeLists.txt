cmake_minimum_required(VERSION 2.8...3.14)

project(ze LANGUAGES C)

set(C_STANDARD 89)

include(CMakeDependentOption)
include(GNUInstallDirs)
cmake_dependent_option(ZE_BUILD_TESTS
  "Build the unit tests when BUILD_TESTING is enabled and we are the root project" ON
  "BUILD_TESTING;CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_CONFIGURATION_TYPES=Debug;Release)
set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/libze-built")

option(LIBUV_BUILD_SHARED "Build shared lib" OFF)
add_subdirectory(deps/libuv)

file(GLOB lib_files
    ${CMAKE_SOURCE_DIR}/src/*.c
)

add_definitions(-DZE_STACK_SIZE=16384)
add_definitions(-DZE_MAIN_STACK=32768)
add_definitions(-DZE_SCRAPE_SIZE=64)
add_definitions(-DHASH_INIT_CAPACITY=512)
add_library(ze STATIC ${lib_files})
target_link_libraries(ze PUBLIC uv_a)
target_include_directories(ze PUBLIC ${CMAKE_SOURCE_DIR}/include )

find_package(OpenSSL)
if(OPENSSL_FOUND)
    target_include_directories(ze BEFORE PUBLIC ${OPENSSL_INCLUDE_DIR})
    add_definitions(-DOPENSSL_USE_STATIC_LIBS=TRUE)
    add_definitions(-DOPENSSL_MSVC_STATIC_RT=TRUE)
    add_definitions(-DNO_GETTIMEOFDAY)
    target_link_libraries(ze PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
    target_link_libraries(ze PUBLIC ${OPENSSL_SSL_LIBRARY})
else()
    add_definitions(-DLIBRESSL_APPS=OFF)
    add_definitions(-DLIBRESSL_TESTS=OFF)
    add_definitions(-DLIBRESSL_DISABLE_OVERRIDE_WINCRYPT_DEFINES_WARNING)
    include_directories(${CMAKE_SOURCE_DIR}/include)
    add_subdirectory(deps/libressl)
    target_link_libraries(ze PUBLIC crypto)
    target_link_libraries(ze PUBLIC ssl)
endif()

target_include_directories(ze
    AFTER PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -D ZE_DEBUG -D ZE_USE_VALGRIND ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fomit-frame-pointer -Wno-return-type")
    find_package(Threads)
    target_link_libraries(ze PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()

if(WIN32)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D ZE_DEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D__PTW32_STATIC_LIB")
    add_definitions(-D__PTW32_STATIC_LIB)
    add_subdirectory(deps/pthreads4w)
    target_link_libraries(ze PUBLIC libpthreadVSE3)
endif()

if(ZE_BUILD_TESTS)
    add_subdirectory(docs/examples)
endif()

if(ZE_BUILD_TESTS)
    set(TARGET_LIST test_map test_reflect)
    foreach (TARGET ${TARGET_LIST})
        add_executable(${TARGET} tests/${TARGET}.c)
        target_link_libraries(${TARGET} ze)
    endforeach()
endif()

set(_fmt TGZ)
if(WIN32)
  set(_fmt ZIP)
endif()

set(CPACK_GENERATOR ${_fmt})
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_NUGET_COMPONENT_INSTALL ON)
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
set(CPACK_VERBATIM_VARIABLES YES)

set(CPACK_PACKAGE_VENDOR "https://github.com/zelang-dev/ze")
set(CPACK_PACKAGE_VERSION 0.0.0)
include(CPack)

set(CMAKE_INSTALL_CONFIG_NAME ${CMAKE_BUILD_TYPE})
install(TARGETS ${ze} DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
