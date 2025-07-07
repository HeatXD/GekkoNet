# Minimum required version of CMake
cmake_minimum_required(VERSION 3.15)

# Project name and version
project(GekkoNet VERSION 1.0)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Option to build both shared and static libraries
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_DOCS "Build documentation" OFF)

if(MSVC)
    if(BUILD_SHARED_LIBS)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
else()
    # GCC/Clang flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

# Option to exclude ASIO from the shared and static libraries
option(NO_ASIO_BUILD "Exclude ASIO from the build" OFF)

# Include directories for your project headers
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty)

if(NOT NO_ASIO_BUILD) 
    include_directories(${PROJECT_SOURCE_DIR}/thirdparty/asio)
endif()

# Manually list all your source files
set(SRC_FILES
    ${PROJECT_SOURCE_DIR}/src/gekkonet.cpp
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
elseif(WIN32 AND NOT NO_ASIO_BUILD)
    target_link_libraries(GekkoNet PRIVATE ws2_32)
endif()

# Documentation configuration
if(BUILD_DOCS)
    find_package(Doxygen
                REQUIRED dot
                OPTIONAL_COMPONENTS mscgen dia)

    # Get all header files
    file(GLOB_RECURSE HEADER_FILES
        ${PROJECT_SOURCE_DIR}/include/*.h
    )

    # Combine sources and headers for documentation
    set(DOXYGEN_INPUT_FILES
        ${SRC_FILES}
        ${HEADER_FILES}
        ${PROJECT_SOURCE_DIR}/README.md
    )

    # Configure Doxygen settings
    set(DOXYGEN_PROJECT_NAME "GekkoNet")
    set(DOXYGEN_PROJECT_VERSION ${PROJECT_VERSION})
    set(DOXYGEN_PROJECT_BRIEF "C/C++ Peer to Peer Game Networking SDK")
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_PRIVATE YES)
    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_SOURCE_BROWSER YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "README.md")
    set(DOXYGEN_COLLABORATION_GRAPH YES)
    set(DOXYGEN_CLASS_GRAPH YES)
    set(DOXYGEN_CALL_GRAPH YES)
    set(DOXYGEN_CALLER_GRAPH YES)
    set(DOXYGEN_HIDE_UNDOC_RELATIONS NO)
    
    # Add custom settings for networking library
    set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES YES)
    set(DOXYGEN_SHOW_INCLUDE_FILES YES)
    set(DOXYGEN_GENERATE_TODOLIST YES)
    set(DOXYGEN_WARNINGS YES)
    set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
    
    # Generate documentation
    doxygen_add_docs(docs
        ${DOXYGEN_INPUT_FILES}
        COMMENT "Generating API documentation with Doxygen"
    )
endif()
