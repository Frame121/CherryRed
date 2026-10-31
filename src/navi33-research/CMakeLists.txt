cmake_minimum_required(VERSION 3.16)

project(navi33_research LANGUAGES CXX)

add_library(navi33_research STATIC
    src/Navi33Research.cpp
    src/Navi33DriverCore.cpp
    src/Navi33BringupPlan.cpp
    src/Navi33ResourceSurvey.cpp
    src/Navi33LinuxResourceParser.cpp
    src/Navi33TargetProfile.cpp
    src/Navi33IpDiscovery.cpp
    src/Navi33BringupModel.cpp
    src/Navi33IntegrationContracts.cpp
    src/Navi33DisplayContracts.cpp
    src/Navi33ComputeContracts.cpp
    src/Navi33Gfx11Manifest.cpp
    src/Navi33Gfx11Bringup.cpp
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

add_executable(navi33_target_profile_tests tests/test_Navi33TargetProfile.cpp)
target_link_libraries(navi33_target_profile_tests PRIVATE navi33_research)

add_executable(navi33_pci_identity_tests tests/test_Navi33PciIdentity.cpp)
target_include_directories(navi33_pci_identity_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../navi33-bringup)
target_compile_features(navi33_pci_identity_tests PRIVATE cxx_std_17)

add_executable(navi33_psp_mailbox_tests tests/test_Navi33PspMailbox.cpp)
target_include_directories(navi33_psp_mailbox_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../navi33-bringup)
target_compile_features(navi33_psp_mailbox_tests PRIVATE cxx_std_17)

add_executable(navi33_psp_load_tests tests/test_Navi33PspLoad.cpp)
target_include_directories(navi33_psp_load_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../navi33-bringup)
target_compile_features(navi33_psp_load_tests PRIVATE cxx_std_17)

add_executable(navi33_client_contracts_tests tests/test_Navi33ClientContracts.cpp)
target_include_directories(navi33_client_contracts_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(navi33_client_contracts_tests PRIVATE cxx_std_17)

add_executable(navi33_ipdiscovery_tests tests/test_Navi33IpDiscovery.cpp)
target_link_libraries(navi33_ipdiscovery_tests PRIVATE navi33_research)
add_executable(navi33_bringup_model_tests tests/test_Navi33BringupModel.cpp)
target_link_libraries(navi33_bringup_model_tests PRIVATE navi33_research)

add_executable(navi33_integration_contract_tests tests/test_Navi33IntegrationContracts.cpp)
target_link_libraries(navi33_integration_contract_tests PRIVATE navi33_research)
add_executable(navi33_display_contract_tests tests/test_Navi33DisplayContracts.cpp)
target_link_libraries(navi33_display_contract_tests PRIVATE navi33_research)
add_executable(navi33_compute_contract_tests tests/test_Navi33ComputeContracts.cpp)
target_link_libraries(navi33_compute_contract_tests PRIVATE navi33_research)
add_executable(navi33_gfx11_manifest_tests tests/test_Navi33Gfx11Manifest.cpp)
target_link_libraries(navi33_gfx11_manifest_tests PRIVATE navi33_research)
add_executable(navi33_gfx11_bringup_tests tests/test_Navi33Gfx11Bringup.cpp)
target_link_libraries(navi33_gfx11_bringup_tests PRIVATE navi33_research)

add_executable(ipdiscovery_bounds_tests
    tests/test_ipdiscovery_bounds.cpp
    ../navi48-bringup/src/ipdiscovery.cpp
)
target_include_directories(ipdiscovery_bounds_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../navi48-bringup/src)
# This target links nothing, so it inherits no C++ standard from another
# target: without an explicit requirement Apple clang compiles it as C++98,
# where the enum-with-underlying-type in ipdiscovery.hpp is ill-formed.
target_compile_features(ipdiscovery_bounds_tests PRIVATE cxx_std_17)

set(navi33_targets
    navi33_research navi33_research_tests navi33_driver_core_tests
    navi33_bringup_plan_tests navi33_resource_survey_tests
    navi33_linux_resource_parser_tests navi33_target_profile_tests
    navi33_pci_identity_tests navi33_psp_mailbox_tests navi33_psp_load_tests
    navi33_client_contracts_tests
    navi33_ipdiscovery_tests
    navi33_bringup_model_tests navi33_integration_contract_tests
    navi33_display_contract_tests navi33_compute_contract_tests
    navi33_gfx11_manifest_tests navi33_gfx11_bringup_tests
    ipdiscovery_bounds_tests)
set_target_properties(${navi33_targets} PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(ipdiscovery_bounds_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../navi48-bringup/src
)

if(MSVC)
    foreach(target IN LISTS navi33_targets)
        target_compile_options(${target} PRIVATE /W4 /WX /GR- /EHs-c-)
    endforeach()
else()
    foreach(target IN LISTS navi33_targets)
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Werror -pedantic -fno-exceptions -fno-rtti
        )
    endforeach()
endif()

enable_testing()
add_test(NAME navi33_research_tests COMMAND navi33_research_tests)
add_test(NAME navi33_driver_core_tests COMMAND navi33_driver_core_tests)
add_test(NAME navi33_bringup_plan_tests COMMAND navi33_bringup_plan_tests)
add_test(NAME navi33_resource_survey_tests COMMAND navi33_resource_survey_tests)
add_test(NAME navi33_linux_resource_parser_tests COMMAND navi33_linux_resource_parser_tests)
add_test(NAME navi33_target_profile_tests COMMAND navi33_target_profile_tests)
add_test(NAME navi33_pci_identity_tests COMMAND navi33_pci_identity_tests)
add_test(NAME navi33_psp_mailbox_tests COMMAND navi33_psp_mailbox_tests)
add_test(NAME navi33_psp_load_tests COMMAND navi33_psp_load_tests)
add_test(NAME navi33_client_contracts_tests COMMAND navi33_client_contracts_tests)
add_test(NAME navi33_ipdiscovery_capture_tests
    COMMAND navi33_ipdiscovery_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/../../RX7600-Linux-Evidence/amdgpu_discovery.bin")
add_test(NAME navi33_bringup_model_tests COMMAND navi33_bringup_model_tests)
add_test(NAME navi33_integration_contract_tests COMMAND navi33_integration_contract_tests)
add_test(NAME navi33_display_contract_tests COMMAND navi33_display_contract_tests)
add_test(NAME navi33_compute_contract_tests COMMAND navi33_compute_contract_tests)
add_test(NAME navi33_gfx11_manifest_tests COMMAND navi33_gfx11_manifest_tests)
add_test(NAME navi33_gfx11_bringup_tests COMMAND navi33_gfx11_bringup_tests)
add_test(NAME ipdiscovery_bounds_tests COMMAND ipdiscovery_bounds_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME firmware_dependency_audit_tests
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_firmware_dependency_audit.py)
    add_test(NAME navi33_firmware_source_trace
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_navi33_firmware_source_trace.py)
    add_test(NAME navi33_firmware_header_bounds
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_navi33_firmware_header_bounds.py)
    add_test(NAME navi48_routing_isolation
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_navi48_isolation.py)
    add_test(NAME navi33_survey_safety
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/../navi33-survey/tests/test_survey_safety.py)
    add_test(NAME navi33_bringup_safety
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/../navi33-bringup/tests/test_bringup_safety.py)
    add_test(NAME navi33_psp_survey_safety
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/../navi33-bringup/tests/test_psp_survey_safety.py)
    add_test(NAME navi33_psp_load_safety
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/../navi33-bringup/tests/test_psp_load_safety.py)
    add_test(NAME navi33_accelerator_safety
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/../navi33-bringup/tests/test_accelerator_safety.py)
endif()
