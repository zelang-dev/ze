cmake_minimum_required(VERSION 2.8...3.14)

project(ze LANGUAGES C)

set(C_STANDARD 89)

include(CMakeDependentOption)
cmake_dependent_option(ZE_BUILD_TESTS
  "Build the unit tests when BUILD_TESTING is enabled and we are the root project" ON
  "BUILD_TESTING;CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_CONFIGURATION_TYPES=Debug;Release)
set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/libze-built")
include_directories( ${CMAKE_SOURCE_DIR}/include )

option(LIBUV_BUILD_SHARED "Build shared lib" OFF)
add_subdirectory(deps/libuv)

file(GLOB lib_files
    ${CMAKE_SOURCE_DIR}/src/*.c
)

add_definitions(-DZE_STACK_SIZE=10240)
add_definitions(-DZE_MAIN_STACK=16384)
add_definitions(-DZE_SCRAPE_SIZE=64)
add_definitions(-DHASH_INIT_CAPACITY=512)
add_library(ze STATIC ${lib_files})
target_link_libraries(ze PUBLIC uv_a)

add_subdirectory(deps/yyjson)
target_link_libraries(ze PUBLIC yyjson)

add_definitions(-DLIBRESSL_APPS=OFF)
add_definitions(-DLIBRESSL_TESTS=OFF)
add_definitions(-DLIBRESSL_DISABLE_OVERRIDE_WINCRYPT_DEFINES_WARNING)
add_subdirectory(deps/libressl)
target_link_libraries(ze PUBLIC ssl)
target_link_libraries(ze PUBLIC crypto)

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
