# Copyright (c) 2019-2022, The Khronos Group Inc.
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
# Author:
#

file(GLOB LOCAL_HEADERS "*.h")
file(GLOB LOCAL_SOURCE "*.cpp")

run_xr_xml_generate(conformance_layer_generator.py gen_dispatch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/template_gen_dispatch.cpp)
run_xr_xml_generate(conformance_layer_generator.py gen_dispatch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/template_gen_dispatch.h)

if(WIN32)
    set(LOCAL_RESOURCE "XrApiLayer_conformance_layer.def")
endif()

add_library(XrApiLayer_runtime_conformance MODULE
    ${LOCAL_SOURCE}
    ${LOCAL_HEADERS}
    ${LOCAL_RESOURCE}
    ${PROJECT_BINARY_DIR}/src/xr_generated_dispatch_table.c
    ${CMAKE_CURRENT_BINARY_DIR}/gen_dispatch.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/gen_dispatch.h
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/conformance_layer.json
    ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_runtime_conformance.json
    @ONLY)

source_group("Headers" FILES ${LOCAL_HEADERS})

add_dependencies(XrApiLayer_runtime_conformance
    generate_openxr_header
    xr_global_generated_files
)

target_include_directories(XrApiLayer_runtime_conformance
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${PROJECT_SOURCE_DIR}/src/common

#for openxr.h:
    PRIVATE ${PROJECT_BINARY_DIR}/include
#for common_config.h:
    PRIVATE ${PROJECT_BINARY_DIR}/src
)

target_link_libraries(XrApiLayer_runtime_conformance Threads::Threads)

if(MSVC)
    # Right now can't build this on MinGW because of directxcolors, etc.
    target_link_libraries(XrApiLayer_runtime_conformance d3d11 d3d12 d3dcompiler dxgi)
else()
    target_compile_definitions(XrApiLayer_runtime_conformance PRIVATE MISSING_DIRECTX_COLORS)
endif()

# Flag generated files
set_source_files_properties(
    ${PROJECT_BINARY_DIR}/src/xr_generated_dispatch_table.c
    # ${PROJECT_BINARY_DIR}/src/conformance/conformance_layer/gen_dispatch.cpp
    # ${PROJECT_BINARY_DIR}/src/conformance/conformance_layer/gen_dispatch.h
    PROPERTIES GENERATED TRUE
)
if(Vulkan_FOUND)
    target_include_directories(XrApiLayer_runtime_conformance
        PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(XrApiLayer_runtime_conformance PRIVATE -Wall)
    target_link_libraries(XrApiLayer_runtime_conformance m)
endif()

if(ANDROID)
    target_link_libraries(XrApiLayer_runtime_conformance ${ANDROID_LOG_LIBRARY})
endif()

if(BUILD_CONFORMANCE_CLI)
    # Copy conformance layer files to conformance_cli binary folder
    add_custom_command(TARGET XrApiLayer_runtime_conformance POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:XrApiLayer_runtime_conformance> $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_runtime_conformance.json $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
    )
endif()

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
