# Copyright (c) 2019-2026 The Khronos Group Inc.
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

run_xr_xml_generate(
    conformance_layer_generator.py gen_dispatch.cpp
    ${PROJECT_SOURCE_DIR}/src/scripts/template_gen_dispatch.cpp
)
run_xr_xml_generate(
    conformance_layer_generator.py gen_dispatch.h
    ${PROJECT_SOURCE_DIR}/src/scripts/template_gen_dispatch.h
)

configure_file(
    conformance_layer.json
    ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_runtime_conformance.json @ONLY
)

add_library(
    XrApiLayer_runtime_conformance MODULE
    Action.cpp
    ActionSet.cpp
    Common.h
    ConformanceHooks.h
    CustomHandleState.h
    D3D11GraphicsValidator.cpp
    GraphicsValidator.cpp
    HandleState.cpp
    HandleState.h
    IGraphicsValidator.h
    Instance.cpp
    Negotiate.cpp
    RuntimeFailure.cpp
    RuntimeFailure.h
    Session.cpp
    Space.cpp
    Swapchain.cpp
    ${PROJECT_BINARY_DIR}/src/xr_generated_dispatch_table.c
    ${CMAKE_CURRENT_BINARY_DIR}/gen_dispatch.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/gen_dispatch.h
)
target_link_libraries(
    XrApiLayer_runtime_conformance PRIVATE Threads::Threads OpenXR::headers
)

source_group("Headers" FILES ${LOCAL_HEADERS})

add_dependencies(XrApiLayer_runtime_conformance xr_common_generated_files)

target_include_directories(
    XrApiLayer_runtime_conformance
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/common
        ${PROJECT_SOURCE_DIR}/src
        #for common_config.h:
        ${PROJECT_BINARY_DIR}/src
        #for xr_generated_dispatch_table_all.h:
        ${CMAKE_CURRENT_BINARY_DIR}/../../api_layers
)

if(MSVC)
    # Right now can't build this on MinGW because of directxcolors, directxmath, etc.
    target_link_libraries(
        XrApiLayer_runtime_conformance
        PRIVATE
            d3d11
            d3d12
            d3dcompiler
            dxgi
    )
endif()

# Flag generated files
# cmake-format: off
set_source_files_properties(
    ${PROJECT_BINARY_DIR}/src/xr_generated_dispatch_table.c
    # ${PROJECT_BINARY_DIR}/src/conformance/conformance_layer/gen_dispatch.cpp
    # ${PROJECT_BINARY_DIR}/src/conformance/conformance_layer/gen_dispatch.h
    PROPERTIES
        GENERATED TRUE
        SKIP_LINTING ON
)
# cmake-format: on

if(XR_USE_GRAPHICS_API_VULKAN)
    if(ANDROID)
        # The vulkan/vk_layer.h header is only available in the NDK 27.2.12479018 toolchain
        # For older NDKs we have to find it from the third party folder
        find_path(
            VK_LAYER_PATH
            NAMES vulkan/vk_layer.h
            PATHS ${ANDROID_NDK}
            HINTS ${ANDROID_NDK}/sources/third_party/vulkan/src/include/
        )
        target_include_directories(
            XrApiLayer_runtime_conformance PRIVATE ${VK_LAYER_PATH}
        )

        # The Android Vulkan Loader needs a dedicated library in order to load the layer.
        add_library(
            VkLayer_xr_runtime_conformance MODULE AndroidVulkanLayer.cpp
        )
        set_target_properties(
            VkLayer_xr_runtime_conformance
            PROPERTIES
                LINK_FLAGS
                "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/VkLayer_xr_runtime_conformance.map\""
        )

        target_include_directories(
            VkLayer_xr_runtime_conformance PRIVATE ${Vulkan_INCLUDE_DIRS}
                                                   ${VK_LAYER_PATH}
        )
    endif()

    target_sources(
        XrApiLayer_runtime_conformance
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/VulkanLayer.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/VulkanGraphicsValidator.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/ApiVulkan.cpp
    )

    target_include_directories(
        XrApiLayer_runtime_conformance PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
    target_link_libraries(
        XrApiLayer_runtime_conformance PRIVATE ${Vulkan_LIBRARY}
    )

    configure_file(
        conformance_vk_layer.json
        ${CMAKE_CURRENT_BINARY_DIR}/VkLayer_OPENXR_xr_runtime_conformance.json
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(XrApiLayer_runtime_conformance PRIVATE -Wall)
    target_link_libraries(XrApiLayer_runtime_conformance PRIVATE m)
endif()

if(ANDROID)
    target_link_libraries(
        XrApiLayer_runtime_conformance PRIVATE ${ANDROID_LOG_LIBRARY}
    )
endif()

# Dynamic Library:
#  - Make build depend on the module definition/version script/export map
#  - Add the linker flag (except windows)

if(XR_USE_GRAPHICS_API_VULKAN)
    set(SYMBOLS_FILE "XrApiLayer_conformance_layer_vk")
else()
    set(SYMBOLS_FILE "XrApiLayer_conformance_layer")
endif()

if(WIN32)
    target_sources(
        XrApiLayer_runtime_conformance
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${SYMBOLS_FILE}.def"
    )
elseif(APPLE)
    set_target_properties(
        XrApiLayer_runtime_conformance
        PROPERTIES
            LINK_FLAGS
            "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/${SYMBOLS_FILE}.expsym"
    )
    target_sources(
        XrApiLayer_runtime_conformance
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${SYMBOLS_FILE}.expsym"
    )
else()
    set_target_properties(
        XrApiLayer_runtime_conformance
        PROPERTIES
            LINK_FLAGS
            "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/${SYMBOLS_FILE}.map\""
    )
    target_sources(
        XrApiLayer_runtime_conformance
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${SYMBOLS_FILE}.map"
    )
endif()

if(BUILD_CONFORMANCE_CLI)
    # Copy the conformance layer configuration to its target directory
    add_custom_command(
        TARGET XrApiLayer_runtime_conformance
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_runtime_conformance.json"
            "$<TARGET_FILE_DIR:XrApiLayer_runtime_conformance>"
    )

    if(XR_USE_GRAPHICS_API_VULKAN AND NOT ANDROID)
        add_custom_command(
            TARGET XrApiLayer_runtime_conformance
            POST_BUILD
            COMMAND
                ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_BINARY_DIR}/VkLayer_OPENXR_xr_runtime_conformance.json
                "$<TARGET_FILE_DIR:XrApiLayer_runtime_conformance>"
        )
        install(
            FILES
                ${CMAKE_CURRENT_BINARY_DIR}/VkLayer_OPENXR_xr_runtime_conformance.json
            DESTINATION conformance
        )
    endif()
endif()

set_target_properties(
    XrApiLayer_runtime_conformance PROPERTIES FOLDER
                                              ${CONFORMANCE_TESTS_FOLDER}
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_runtime_conformance.json
    DESTINATION conformance
)

install(
    TARGETS XrApiLayer_runtime_conformance
    LIBRARY DESTINATION conformance
    ARCHIVE DESTINATION conformance
    RUNTIME DESTINATION conformance
)

if(MSVC AND INSTALL_SYMBOLS)
    install(
        FILES $<TARGET_PDB_FILE:XrApiLayer_runtime_conformance>
        DESTINATION conformance
        OPTIONAL
    )
endif()
