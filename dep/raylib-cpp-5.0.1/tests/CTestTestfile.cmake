# CMake generated Testfile for 
# Source directory: F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests
# Build directory: F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(raylib_cpp_test "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/Debug/raylib_cpp_test.exe")
  set_tests_properties(raylib_cpp_test PROPERTIES  _BACKTRACE_TRIPLES "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/CMakeLists.txt;17;add_test;F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(raylib_cpp_test "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/Release/raylib_cpp_test.exe")
  set_tests_properties(raylib_cpp_test PROPERTIES  _BACKTRACE_TRIPLES "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/CMakeLists.txt;17;add_test;F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(raylib_cpp_test "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/MinSizeRel/raylib_cpp_test.exe")
  set_tests_properties(raylib_cpp_test PROPERTIES  _BACKTRACE_TRIPLES "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/CMakeLists.txt;17;add_test;F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(raylib_cpp_test "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/RelWithDebInfo/raylib_cpp_test.exe")
  set_tests_properties(raylib_cpp_test PROPERTIES  _BACKTRACE_TRIPLES "F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/CMakeLists.txt;17;add_test;F:/Downloads/raylib-cpp-5.0.1/raylib-cpp-5.0.1/tests/CMakeLists.txt;0;")
else()
  add_test(raylib_cpp_test NOT_AVAILABLE)
endif()
