cmake_minimum_required(VERSION 3.16)

project(navi33_research LANGUAGES CXX)

add_library(navi33_research STATIC
    src/Navi33Research.cpp
    src/Navi33DriverCore.cpp
    src/Navi33BringupPlan.cpp
    src/Navi33ResourceSurvey.cpp
    src/Navi33LinuxResourceParser.cpp
)
target_include_directories(navi33_research PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(navi33_research PUBLIC cxx_std_17)

add_executable(navi33_research_tests tests/test_Navi33Research.cpp)
target_link_libraries(navi33_research_tests PRIVATE navi33_research)

add_executable(navi33_driver_core_tests tests/test_Navi33DriverCore.cpp)
target_link_libraries(navi33_driver_core_tests PRIVATE navi33_research)

add_executable(navi33_bringup_plan_tests tests/test_Navi33BringupPlan.cpp)
target_link_libraries(navi33_bringup_plan_tests PRIVATE navi33_research)

add_executable(navi33_resource_survey_tests tests/test_Navi33ResourceSurvey.cpp)
target_link_libraries(navi33_resource_survey_tests PRIVATE navi33_research)

add_executable(navi33_linux_resource_parser_tests tests/test_Navi33LinuxResourceParser.cpp)
target_link_libraries(navi33_linux_resource_parser_tests PRIVATE navi33_research)

add_executable(ipdiscovery_bounds_tests
    tests/test_ipdiscovery_bounds.cpp
    ../navi48-bringup/src/ipdiscovery.cpp
)
target_include_directories(ipdiscovery_bounds_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../navi48-bringup/src)

set(navi33_targets
    navi33_research navi33_research_tests navi33_driver_core_tests
    navi33_bringup_plan_tests navi33_resource_survey_tests
    navi33_linux_resource_parser_tests ipdiscovery_bounds_tests)
set_target_properties(${navi33_targets} PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    foreach(target IN LISTS navi33_targets)
        target_compile_options(${target} PRIVATE /W4 /WX /GR- /EHs-c-)
    endforeach()
else()
    foreach(target IN LISTS navi33_targets)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti)
    endforeach()
endif()

enable_testing()
add_test(NAME navi33_research_tests COMMAND navi33_research_tests)
add_test(NAME navi33_driver_core_tests COMMAND navi33_driver_core_tests)
add_test(NAME navi33_bringup_plan_tests COMMAND navi33_bringup_plan_tests)
add_test(NAME navi33_resource_survey_tests COMMAND navi33_resource_survey_tests)
add_test(NAME navi33_linux_resource_parser_tests COMMAND navi33_linux_resource_parser_tests)
add_test(NAME ipdiscovery_bounds_tests COMMAND ipdiscovery_bounds_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME firmware_dependency_audit_tests
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_firmware_dependency_audit.py)
    add_test(NAME navi48_routing_isolation
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_navi48_isolation.py)
    add_test(NAME navi33_survey_safety
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/../navi33-survey/tests/test_survey_safety.py)
endif()
