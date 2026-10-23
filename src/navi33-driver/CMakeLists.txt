cmake_minimum_required(VERSION 3.16)
project(navi33_driver LANGUAGES CXX)

add_library(navi33_driver STATIC src/Firmware.cpp src/VramArena.cpp src/Preparation.cpp)
target_include_directories(navi33_driver PUBLIC include)
target_compile_features(navi33_driver PUBLIC cxx_std_17)
set_target_properties(navi33_driver PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(navi33_driver PRIVATE /W4 /WX /GR- /EHs-c-)
else()
    target_compile_options(navi33_driver PRIVATE -Wall -Wextra -Werror -pedantic
        -fno-exceptions -fno-rtti -ffreestanding)
endif()

add_executable(navi33_firmware_inspect tools/firmware_inspect.cpp)
target_link_libraries(navi33_firmware_inspect PRIVATE navi33_driver)

enable_testing()
foreach(component firmware memory preparation)
    add_executable(navi33_${component}_tests tests/test_${component}.cpp)
    target_link_libraries(navi33_${component}_tests PRIVATE navi33_driver)
    target_include_directories(navi33_${component}_tests PRIVATE tests)
    if(MSVC)
        target_compile_options(navi33_${component}_tests PRIVATE /W4 /WX)
    else()
        target_compile_options(navi33_${component}_tests PRIVATE -Wall -Wextra -Werror -pedantic)
    endif()
    add_test(NAME navi33_${component}_tests COMMAND navi33_${component}_tests)
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME navi33_firmware_cli
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py
        $<TARGET_FILE:navi33_firmware_inspect>)
