# Minimum required version of CMake
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(GekkoNet VERSION 1.0)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Option to build both shared and static libraries
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

if(MSVC)
    # Visual Studio specific flags
    if(BUILD_SHARED_LIBS)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    endif()
else()
    # GCC/Clang flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

# Option to exclude ASIO from the shared and static libraries
option(NO_ASIO_BUILD "Exclude ASIO from the build" OFF)

# Include directories for your project headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Include directories for third-party header-only libraries
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/asio)

# Manually list all your source files
set(SRC_FILES
    ${PROJECT_SOURCE_DIR}/src/backend.cpp
    ${PROJECT_SOURCE_DIR}/src/event.cpp
    ${PROJECT_SOURCE_DIR}/src/gekko.cpp
    ${PROJECT_SOURCE_DIR}/src/input.cpp
    ${PROJECT_SOURCE_DIR}/src/net.cpp
    ${PROJECT_SOURCE_DIR}/src/player.cpp
    ${PROJECT_SOURCE_DIR}/src/storage.cpp
    ${PROJECT_SOURCE_DIR}/src/sync.cpp
)

# Shared libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/out)

# Static libraries
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/out)

# Create both static and shared library targets
add_library(GekkoNet ${SRC_FILES})

# Set the output names conditionally based on used options
set(TARGET_SUFFIX "")

if(NOT BUILD_SHARED_LIBS)
    set(TARGET_SUFFIX "${TARGET_SUFFIX}_STATIC")
endif()

if(NO_ASIO_BUILD)
    set(TARGET_SUFFIX "${TARGET_SUFFIX}_NO_ASIO")
endif()

# Set the output name with the conditional suffix
set_target_properties(GekkoNet PROPERTIES OUTPUT_NAME "GekkoNet${TARGET_SUFFIX}")

# Set properties for Windows DLL export macros (if building a DLL)
if(WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(GekkoNet PRIVATE GEKKONET_DLL_EXPORT)
else()
    target_compile_definitions(GekkoNet PRIVATE GEKKONET_STATIC)
endif()

if(NO_ASIO_BUILD)
    target_compile_definitions(GekkoNet PRIVATE GEKKONET_NO_ASIO)
endif()