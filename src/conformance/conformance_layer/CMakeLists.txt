# Copyright (c) 2019-2023, The Khronos Group Inc.
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
    ${LOCAL_SOURCE}
    ${LOCAL_HEADERS}
    XrApiLayer_conformance_layer.def
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
set_source_files_properties(
    ${PROJECT_BINARY_DIR}/src/xr_generated_dispatch_table.c
    # ${PROJECT_BINARY_DIR}/src/conformance/conformance_layer/gen_dispatch.cpp
    # ${PROJECT_BINARY_DIR}/src/conformance/conformance_layer/gen_dispatch.h
    PROPERTIES GENERATED TRUE
)
if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(
        XrApiLayer_runtime_conformance PRIVATE ${Vulkan_INCLUDE_DIRS}
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

if(APPLE)
    set_target_properties(
        XrApiLayer_runtime_conformance
        PROPERTIES
            LINK_FLAGS
            "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_conformance_layer.expsym"
    )
elseif(NOT WIN32)
    set_target_properties(
        XrApiLayer_runtime_conformance
        PROPERTIES LINK_FLAGS "-Wl,-Bsymbolic,--exclude-libs,ALL"
    )
endif()

if(BUILD_CONFORMANCE_CLI)
    # Copy conformance layer files to conformance_cli binary folder
    add_custom_command(
        TARGET XrApiLayer_runtime_conformance
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:XrApiLayer_runtime_conformance>
            $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
        COMMAND
            ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_runtime_conformance.json
            $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
    )
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

if(MSVC)
    install(
        FILES $<TARGET_PDB_FILE:XrApiLayer_runtime_conformance>
        DESTINATION conformance
        OPTIONAL
    )
endif()
