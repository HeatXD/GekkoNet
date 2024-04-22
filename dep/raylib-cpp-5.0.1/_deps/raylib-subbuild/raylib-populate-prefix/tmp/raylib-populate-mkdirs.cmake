# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/_deps/raylib-src"
  "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/_deps/raylib-build"
  "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/_deps/raylib-subbuild/raylib-populate-prefix"
  "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/_deps/raylib-subbuild/raylib-populate-prefix/tmp"
  "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/_deps/raylib-subbuild/raylib-populate-prefix/src/raylib-populate-stamp"
  "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/_deps/raylib-subbuild/raylib-populate-prefix/src"
  "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/_deps/raylib-subbuild/raylib-populate-prefix/src/raylib-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/_deps/raylib-subbuild/raylib-populate-prefix/src/raylib-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/_deps/raylib-subbuild/raylib-populate-prefix/src/raylib-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
