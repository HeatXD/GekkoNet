# Install script for directory: F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/raylib_cpp")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/AudioDevice.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/AudioStream.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/AutomationEventList.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/BoundingBox.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Camera2D.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Camera3D.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Color.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Font.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Functions.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Gamepad.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Image.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Material.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Matrix.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Mesh.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Model.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/ModelAnimation.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Mouse.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Music.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Ray.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/RayCollision.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/RaylibException.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/raylib-cpp-utils.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/raylib-cpp.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/raylib.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/raymath.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Rectangle.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/RenderTexture.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Shader.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Sound.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Text.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Texture.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/TextureUnmanaged.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Touch.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Vector2.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Vector3.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Vector4.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/VrStereoConfig.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Wave.hpp"
    "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/include/Window.hpp"
    )
endif()

