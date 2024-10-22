# Minimum required version of CMake
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(GekkoNet VERSION 1.0)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Option to build both shared and static libraries
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# Include directories for your project headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Include directories for third-party header-only libraries
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/asio)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/zpp)

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

# Create both static and shared library targets
add_library(GekkoNet ${SRC_FILES})

# Set properties for Windows DLL export macros (if building a DLL)
if(WIN32 AND BUILD_SHARED_LIBS)
    target_compile_definitions(GekkoNet PRIVATE GEKKONET_DLL_EXPORT)
endif()
