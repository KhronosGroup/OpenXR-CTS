# Copyright (c) 2019-2024, The Khronos Group Inc.
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

configure_file(
    conformance_test_layer.json
    ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_conformance_test_layer.json @ONLY
)

add_library(
    XrApiLayer_conformance_test_layer MODULE
    ${LOCAL_SOURCE}
    ${LOCAL_HEADERS}
)
target_link_libraries(
    XrApiLayer_conformance_test_layer PRIVATE OpenXR::headers
)

source_group("Headers" FILES ${LOCAL_HEADERS})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(XrApiLayer_conformance_test_layer PRIVATE -Wall)
    target_link_libraries(XrApiLayer_conformance_test_layer PRIVATE m)
endif()

if(ANDROID)
    target_link_libraries(
        XrApiLayer_conformance_test_layer PRIVATE ${ANDROID_LOG_LIBRARY}
    )
endif()

# Dynamic Library:
#  - Make build depend on the module definition/version script/export map
#  - Add the linker flag (except windows)
if(WIN32)
    target_sources(
        XrApiLayer_conformance_test_layer
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_conformance_test_layer.def"
    )
elseif(APPLE)
    set_target_properties(
        XrApiLayer_conformance_test_layer
        PROPERTIES
            LINK_FLAGS
            "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_conformance_test_layer.expsym"
    )
    target_sources(
        XrApiLayer_conformance_test_layer
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_conformance_test_layer.expsym"
    )
else()
    set_target_properties(
        XrApiLayer_conformance_test_layer
        PROPERTIES
            LINK_FLAGS
            "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_conformance_test_layer.map\""
    )
    target_sources(
        XrApiLayer_conformance_test_layer
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/XrApiLayer_conformance_test_layer.map"
    )
endif()

if(BUILD_CONFORMANCE_CLI)
    # Copy conformance layer files to conformance_cli binary folder
    add_custom_command(
        TARGET XrApiLayer_conformance_test_layer
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:XrApiLayer_conformance_test_layer>
            $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
        COMMAND
            ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_conformance_test_layer.json
            $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
    )
endif()

set_target_properties(
    XrApiLayer_conformance_test_layer PROPERTIES FOLDER
                                              ${CONFORMANCE_TESTS_FOLDER}
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_conformance_test_layer.json
    DESTINATION conformance
)

install(
    TARGETS XrApiLayer_conformance_test_layer
    LIBRARY DESTINATION conformance
    ARCHIVE DESTINATION conformance
    RUNTIME DESTINATION conformance
)

if(MSVC)
    install(
        FILES $<TARGET_PDB_FILE:XrApiLayer_conformance_test_layer>
        DESTINATION conformance
        OPTIONAL
    )
endif()
