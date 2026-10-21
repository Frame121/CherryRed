cmake_minimum_required(VERSION 3.16)

project(navi33_research LANGUAGES CXX)

add_library(navi33_research STATIC
    src/Navi33Research.cpp
    src/Navi33DriverCore.cpp
)
target_include_directories(navi33_research
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(navi33_research PUBLIC cxx_std_17)

add_executable(navi33_research_tests
    tests/test_Navi33Research.cpp
)
target_link_libraries(navi33_research_tests PRIVATE navi33_research)

add_executable(navi33_driver_core_tests
    tests/test_Navi33DriverCore.cpp
)
target_link_libraries(navi33_driver_core_tests PRIVATE navi33_research)

add_executable(ipdiscovery_bounds_tests
    tests/test_ipdiscovery_bounds.cpp
    ../navi48-bringup/src/ipdiscovery.cpp
)
target_include_directories(ipdiscovery_bounds_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../navi48-bringup/src
)

set_target_properties(navi33_research navi33_research_tests PROPERTIES
    CXX_EXTENSIONS OFF
)
set_target_properties(navi33_driver_core_tests ipdiscovery_bounds_tests PROPERTIES
    CXX_EXTENSIONS OFF
)

if(MSVC)
    target_compile_options(navi33_research PRIVATE /W4 /WX /GR- /EHs-c-)
    target_compile_options(navi33_research_tests PRIVATE /W4 /WX /GR- /EHs-c-)
    target_compile_options(navi33_driver_core_tests PRIVATE /W4 /WX /GR- /EHs-c-)
    target_compile_options(ipdiscovery_bounds_tests PRIVATE /W4 /WX /GR- /EHs-c-)
else()
    target_compile_options(navi33_research PRIVATE
        -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti
    )
    target_compile_options(navi33_research_tests PRIVATE
        -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti
    )
    target_compile_options(navi33_driver_core_tests PRIVATE
        -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti
    )
    target_compile_options(ipdiscovery_bounds_tests PRIVATE
        -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti
    )
endif()

enable_testing()
add_test(NAME navi33_research_tests COMMAND navi33_research_tests)
add_test(NAME navi33_driver_core_tests COMMAND navi33_driver_core_tests)
add_test(NAME ipdiscovery_bounds_tests COMMAND ipdiscovery_bounds_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME navi48_routing_isolation
        COMMAND
            ${Python3_EXECUTABLE} -B
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_navi48_isolation.py
    )
endif()
