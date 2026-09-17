cmake_minimum_required(VERSION 3.19)
include(CheckIPOSupported)
include(CheckSymbolExists)
include(FetchContent)

project(_memalloc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compile options
add_compile_options(-fPIC -fvisibility=hidden -pthread -Wall -Wextra)

# Platform-specific compile definitions
if (APPLE)
    # Fix for newer macOS SDKs that don't define BSD-style types. These are needed by Abseil on macOS
    add_compile_definitions(_DARWIN_C_SOURCE)
endif ()

add_compile_definitions(_POSIX_C_SOURCE=200809L)

# Abseil: HeapMapType is backed by absl::flat_hash_map (Swiss Tables). absl is built as
# static libraries (no BUILD_SHARED_LIBS), position-independent so it can be merged into
# the final shared Python extension. Tag matches dd-trace-py's pinned version.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ABSL_USE_GOOGLETEST_HEAD OFF CACHE BOOL "" FORCE)
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "" FORCE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
FetchContent_Declare(
        absl
        GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
        GIT_TAG 20250127.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(absl)

# Find Python (be flexible about what's available in build environments)
find_package(Python3 COMPONENTS Interpreter Development)

# Make sure we have necessary Python variables
if (NOT Python3_INCLUDE_DIRS)
    message(FATAL_ERROR "Python3_INCLUDE_DIRS not found")
endif ()

set(CMAKE_REQUIRED_INCLUDES ${Python3_INCLUDE_DIRS})
unset(PYROSCOPE_GIL_DISABLED CACHE)
check_symbol_exists(Py_GIL_DISABLED "Python.h" PYROSCOPE_GIL_DISABLED)
unset(CMAKE_REQUIRED_INCLUDES)
if (PYROSCOPE_GIL_DISABLED)
    message(FATAL_ERROR "free-threaded CPython is unsupported: ${Python3_EXECUTABLE} defines Py_GIL_DISABLED. Build against a GIL-enabled interpreter.")
endif ()

message(STATUS "Python3_EXECUTABLE ${Python3_EXECUTABLE}")
message(STATUS "Python3_VERSION ${Python3_VERSION}")
message(STATUS "Python3_INCLUDE_DIRS ${Python3_INCLUDE_DIRS}")

# Python3::Python target might not exist in all build environments so we'll link using include dirs and let the linker
# find Python dynamically
if (NOT TARGET Python3::Python)
    message(STATUS "Python3::Python target not found, using include dirs only")
endif ()

set(COLLECTOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ddtrace/profiling/collector)
set(PROFILING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ddtrace/internal/datadog/profiling)

set(SOURCE_FILES
        ${COLLECTOR_DIR}/_memalloc.cpp
        ${COLLECTOR_DIR}/_memalloc_tb.cpp
        ${COLLECTOR_DIR}/_memalloc_heap.cpp
        ${COLLECTOR_DIR}/_memalloc_reentrant.cpp
        pyroscope/Pyroscope.h
        ${PROFILING_DIR}/stack/src/echion/danger.cc
        ${PROFILING_DIR}/stack/src/echion/frame.cc
        ${PROFILING_DIR}/stack/src/echion/greenlets.cc
        ${PROFILING_DIR}/stack/src/echion/interp.cc
        ${PROFILING_DIR}/stack/src/echion/long.cc
        ${PROFILING_DIR}/stack/src/echion/mirrors.cc
        ${PROFILING_DIR}/stack/src/echion/stack_chunk.cc
        ${PROFILING_DIR}/stack/src/echion/stacks.cc
        ${PROFILING_DIR}/stack/src/echion/strings.cc
        ${PROFILING_DIR}/stack/src/echion/tasks.cc
        ${PROFILING_DIR}/stack/src/echion/threads.cc
        ${PROFILING_DIR}/stack/src/echion/vm.cc
        ${PROFILING_DIR}/stack/src/sampler.cpp
        ${PROFILING_DIR}/stack/src/stack.cpp
        ${PROFILING_DIR}/stack/src/stack_renderer.cpp
        ${PROFILING_DIR}/stack/src/thread_span_links.cpp)

set(PYROSCOPE_FFI_INCLUDE_DIR "" CACHE PATH "Directory containing pyroscope_ffi.h")
if (NOT PYROSCOPE_FFI_INCLUDE_DIR)
    message(FATAL_ERROR "PYROSCOPE_FFI_INCLUDE_DIR must be set")
endif ()

set(FULL_EXTENSION_NAME "datadog_mem_profiler")

add_library(${FULL_EXTENSION_NAME} STATIC ${SOURCE_FILES})
if(APPLE)
    target_compile_definitions(${FULL_EXTENSION_NAME} PRIVATE PL_DARWIN)
elseif(UNIX)
    target_compile_definitions(${FULL_EXTENSION_NAME} PRIVATE PL_LINUX)
endif()


# Set properties to prevent CMake from adding any prefix or suffix
#set_target_properties(${FULL_EXTENSION_NAME} PROPERTIES PREFIX "" SUFFIX "")

# Set output directory if specified
if (DEFINED LIB_INSTALL_DIR)
    set_target_properties(${FULL_EXTENSION_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${LIB_INSTALL_DIR})
endif ()

# Include directories
target_include_directories(
        ${FULL_EXTENSION_NAME}
        PRIVATE ${Python3_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/pyroscope
        ${COLLECTOR_DIR}
        ${PROFILING_DIR}
        ${PYROSCOPE_FFI_INCLUDE_DIR}
        ${PROFILING_DIR}/stack/echion
        ${PROFILING_DIR}/stack/include
)

# Link libraries Python3::Python target might not exist in all build environments (e.g., manylinux) Python modules
# should use -undefined dynamic_lookup on macOS and not link to libpython on Linux
if (TARGET Python3::Python AND NOT APPLE)
    target_link_libraries(${FULL_EXTENSION_NAME} PRIVATE Python3::Python)
endif ()

# Compile against absl::flat_hash_map (brings in include dirs and records absl as a link
# dependency). Since this is a STATIC library, the absl object code is not archived in here;
# we merge it explicitly via bundle_static_library below.
target_link_libraries(${FULL_EXTENSION_NAME} PRIVATE absl::flat_hash_map)

include(${CMAKE_CURRENT_SOURCE_DIR}/BundleStaticLibrary.cmake)

bundle_static_library(${FULL_EXTENSION_NAME} ${FULL_EXTENSION_NAME}_bundled)
