# Copyright (c) 2020-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

if(APPLE)
    # Force all compilers to output to binary folder without additional output (like Windows adds "Debug" and "Release" folders)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
            ${CMAKE_CURRENT_BINARY_DIR}
        )
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
            ${CMAKE_CURRENT_BINARY_DIR}
        )
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG}
            ${CMAKE_CURRENT_BINARY_DIR}
        )
    endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)
endif()

file(
    GLOB
    LOCAL_HEADERS
    CONFIGURE_DEPENDS
    "*.h"
)
file(
    GLOB
    LOCAL_SOURCE
    CONFIGURE_DEPENDS
    "*.cpp"
)

add_executable(conformance_cli ${LOCAL_SOURCE} ${LOCAL_HEADERS})
add_sanitizers(conformance_cli)

source_group("Headers" FILES ${LOCAL_HEADERS})

add_dependencies(conformance_cli conformance_test)

target_link_libraries(conformance_cli conformance_test)

target_include_directories(
    conformance_cli
    PRIVATE
        ../conformance_test
        ../framework
        ${PROJECT_SOURCE_DIR}/src/common
        ${PROJECT_SOURCE_DIR}/src/external
)

target_include_directories(
    conformance_cli PRIVATE ${PROJECT_SOURCE_DIR}/src
                            ${PROJECT_SOURCE_DIR}/src/common
)

if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(conformance_cli PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

# Be able to find .so when installed
set_property(TARGET conformance_cli PROPERTY INSTALL_RPATH $ORIGIN)

set_target_properties(
    conformance_cli PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER}
)

install(TARGETS conformance_cli RUNTIME DESTINATION conformance)

# Copy conformance_test assets files to the conformance_cli binary folder
foreach(ASSET ${CONFORMANCE_ASSETS})
    get_filename_component(ASSET_FN "${ASSET}" NAME)
    add_custom_command(
        TARGET conformance_cli
        PRE_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:conformance_cli>"
        COMMAND
            "${CMAKE_COMMAND}" -E copy "${ASSET}"
            "$<TARGET_FILE_DIR:conformance_cli>"
        VERBATIM
        COMMENT "Copying conformance test asset ${ASSET_FN}"
    )
    install(FILES "${ASSET}" DESTINATION conformance)
endforeach()

# Copy conformance test binary to conformance_cli binary folder
add_custom_command(
    TARGET conformance_cli
    PRE_BUILD
    COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:conformance_cli>"
    COMMAND
        "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:conformance_test>"
        "$<TARGET_FILE_DIR:conformance_cli>"
    VERBATIM
    COMMENT
        "Copying conformance test library binary $<TARGET_FILE_NAME:conformance_test>"
)

# Copy conformance layer files to conformance_cli binary folder
add_custom_command(
    TARGET conformance_cli
    PRE_BUILD
    COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:conformance_cli>"
    COMMAND
        "${CMAKE_COMMAND}" -E copy
        "$<TARGET_FILE:XrApiLayer_runtime_conformance>"
        "$<TARGET_FILE_DIR:conformance_cli>"
    COMMAND
        "${CMAKE_COMMAND}" -E copy
        "$<TARGET_FILE_DIR:XrApiLayer_runtime_conformance>/XrApiLayer_runtime_conformance.json"
        "$<TARGET_FILE_DIR:conformance_cli>"
    VERBATIM
    COMMENT
        "Copying conformance test layer binary $<TARGET_FILE_NAME:conformance_test>"
)
install(
    FILES
        "$<TARGET_FILE_DIR:XrApiLayer_runtime_conformance>/XrApiLayer_runtime_conformance.json"
    DESTINATION conformance
)

if(ANDROID AND XR_USE_GRAPHICS_API_VULKAN)
    # Copy Vulkan XR conformance layer file to the conformance_cli binary folder
    add_custom_command(
        TARGET conformance_cli
        PRE_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:conformance_cli>"
        COMMAND
            "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:VkLayer_xr_runtime_conformance>"
            "$<TARGET_FILE_DIR:conformance_cli>"
        VERBATIM
        COMMENT
            "Copying Vulkan conformance test layer binary $<TARGET_FILE_NAME:conformance_test>"
    )

endif()

if(XR_USE_GRAPHICS_API_VULKAN)
    add_custom_command(
        TARGET conformance_cli
        PRE_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:conformance_cli>"
        COMMAND
            "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE_DIR:XrApiLayer_runtime_conformance>/VkLayer_OPENXR_xr_runtime_conformance.json"
            "$<TARGET_FILE_DIR:conformance_cli>"
        VERBATIM
        COMMENT
            "Copying conformance test layer manifest VkLayer_OPENXR_xr_runtime_conformance.json"
    )
    install(
        FILES
            "$<TARGET_FILE_DIR:XrApiLayer_runtime_conformance>/VkLayer_OPENXR_xr_runtime_conformance.json"
        DESTINATION conformance
    )
endif()

# Copy test layer files to conformance_cli binary folder
add_custom_command(
    TARGET conformance_cli
    PRE_BUILD
    COMMAND
        "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:conformance_cli>"
    # the code for this is embedded in the main test shared object
    COMMAND
        "${CMAKE_COMMAND}" -E copy
        "$<TARGET_FILE_DIR:conformance_test>/XrApiLayer_conformance_test_layer.json"
        "$<TARGET_FILE_DIR:conformance_cli>"
    VERBATIM
    COMMENT
        "Copying conformance test layer manifest XrApiLayer_conformance_test_layer.json"
)
install(
    FILES
        "$<TARGET_FILE_DIR:conformance_test>/XrApiLayer_conformance_test_layer.json"
    DESTINATION conformance
)

if(DYNAMIC_LOADER)
    add_custom_command(
        TARGET conformance_cli
        PRE_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:conformance_cli>"
        COMMAND
            "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:openxr_loader>"
            "$<TARGET_FILE_DIR:conformance_cli>"
        VERBATIM
        COMMENT "Copying loader binary $<TARGET_FILE_NAME:openxr_loader>"
    )
endif()

if(MSVC)
    set_property(
        TARGET conformance_cli
        PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:conformance_cli>"
    )
endif()

if(MSVC AND INSTALL_SYMBOLS)
    install(
        FILES $<TARGET_PDB_FILE:conformance_cli>
        DESTINATION conformance
        OPTIONAL
    )
endif()

# Install the README
install(FILES ../conformance_test/readme.md DESTINATION conformance)

add_test(
    NAME conformance_cli
    COMMAND conformance_cli -E XR_MND_headless exclude:[interactive]
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:conformance_cli>"
)

set_tests_properties(
    conformance_cli
    PROPERTIES
        TIMEOUT 60
        ENVIRONMENT XR_RUNTIME_JSON=${PROJECT_BINARY_DIR}/src/tests/test_runtimes/openxr/1/active_runtime.json
)

add_dependencies(conformance_cli test_runtime)

# Run the CTS with api_dump layer enabled to verify layer compatibility if the layer is
# available. Note: Layer is not part of OpenXR-CTS repo.
if(TARGET XrApiLayer_api_dump)
    gen_xr_layer_json(
        "${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_api_dump.json"
        LUNARG_api_dump
        "$<TARGET_FILE:XrApiLayer_api_dump>"
        1
        "API Layer to record api calls as they occur"
        ""
    )

    add_custom_command(
        TARGET conformance_cli
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_api_dump.json"
            "$<TARGET_FILE_DIR:conformance_cli>"
        VERBATIM
        COMMENT
            "Copying conformance api_dump layer manifest XrApiLayer_api_dump.json"
    )

    # Add generated file to our sources so we depend on it, and thus trigger generation.
    target_sources(
        conformance_cli
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_api_dump.json"
    )

    add_test(
        NAME conformance_cli_api_dump
        COMMAND conformance_cli -E XR_MND_headless exclude:[interactive]
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:conformance_cli>"
    )

    set_tests_properties(
        conformance_cli_api_dump
        PROPERTIES
            TIMEOUT
            60
            ENVIRONMENT
            "XR_RUNTIME_JSON=${PROJECT_BINARY_DIR}/src/tests/test_runtimes/openxr/1/active_runtime.json;XR_ENABLE_API_LAYERS=XR_APILAYER_LUNARG_api_dump;XR_API_DUMP_EXPORT_TYPE=html"
    )

    add_dependencies(conformance_cli XrApiLayer_api_dump)
endif()

# Run the CTS with core_validation layer enabled to verify layer compatibility, if the layer is
# available. Note: Layer is not part of OpenXR-CTS repo.
if(TARGET XrApiLayer_core_validation)
    gen_xr_layer_json(
        "${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_core_validation.json"
        LUNARG_core_validation
        "$<TARGET_FILE:XrApiLayer_core_validation>"
        1
        "API Layer to perform validation of api calls and parameters as they occur"
        ""
    )

    add_custom_command(
        TARGET conformance_cli
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_core_validation.json"
            "$<TARGET_FILE_DIR:conformance_cli>"
        VERBATIM
        COMMENT
            "Copying conformance core_validation layer manifest XrApiLayer_core_validation.json"
    )

    # Add generated file to our sources so we depend on it, and thus trigger generation.
    target_sources(
        conformance_cli
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_core_validation.json"
    )

    add_test(
        NAME conformance_cli_core_validation
        COMMAND conformance_cli -E XR_MND_headless exclude:[interactive]
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:conformance_cli>"
    )

    set_tests_properties(
        conformance_cli_core_validation
        PROPERTIES
            TIMEOUT
            60
            ENVIRONMENT
            "XR_RUNTIME_JSON=${PROJECT_BINARY_DIR}/src/tests/test_runtimes/openxr/1/active_runtime.json;XR_ENABLE_API_LAYERS=XR_APILAYER_LUNARG_core_validation"
    )

    add_dependencies(conformance_cli XrApiLayer_core_validation)
endif()
