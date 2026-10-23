cmake_minimum_required(VERSION 3.16)
project(navi33_probe_host_tests LANGUAGES CXX)

# Host regression tests only, not a kext build or an Apple ABI check.
add_executable(navi33_probe_tests tests/test_probe.cpp src/Navi33Probe.cpp)
target_include_directories(navi33_probe_tests PRIVATE tests/shim src)
target_compile_features(navi33_probe_tests PRIVATE cxx_std_17)
set_target_properties(navi33_probe_tests PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(navi33_probe_tests PRIVATE /W4 /WX)
else()
    target_compile_options(navi33_probe_tests PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

enable_testing()
add_test(NAME navi33_probe_tests COMMAND navi33_probe_tests)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME navi33_probe_manifest
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_manifest.py)
