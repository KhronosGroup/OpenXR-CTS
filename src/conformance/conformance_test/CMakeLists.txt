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
#

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
file(
    GLOB
    ASSETS
    "composition_assets/*.png"
    "composition_examples/*.jpg"
    "composition_examples/*.png"
    "SourceCodePro-Regular.otf"
    "gltf_examples/*.glb"
    "gltf_examples/*.png"
    "pbr_assets/*.png"
    "d3d_shaders/*.hlsl"
)
file(GLOB VULKAN_SHADERS "vulkan_shaders/*.glsl")

configure_file(
    conformance_test_layer.json
    ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_conformance_test_layer.json @ONLY
)

# Check to see if git-lfs is working right
file(STRINGS gltf_examples/VertexColorTest.glb LFS_CHECK_STRINGS)
if(LFS_CHECK_STRINGS MATCHES "https://git-lfs[.]github[.]com/spec/v1")
    message(
        FATAL_ERROR
            "Found a git-lfs pointer file instead of the binary file that should replace it. Please install git-lfs, run 'git lfs install', 'git lfs checkout', and 'git lfs fetch'"
    )
endif()

add_library(
    conformance_test SHARED
    ${LOCAL_HEADERS}
    ${LOCAL_SOURCE}
    ${VULKAN_SHADERS}
    ${PROJECT_SOURCE_DIR}/src/common/platform_utils.hpp
)

source_group("Header Files" FILES ${LOCAL_HEADERS})
source_group("Source Files" FILES ${LOCAL_SOURCE})
target_link_libraries(
    conformance_test
    PRIVATE conformance_utilities conformance_framework
    PUBLIC OpenXR::headers
)

add_dependencies(conformance_test XrApiLayer_runtime_conformance)
openxr_add_filesystem_utils(conformance_test)

target_include_directories(
    conformance_test
    PRIVATE
        # For including compiled shaders
        ${CMAKE_CURRENT_BINARY_DIR}
        ../framework
        ${PROJECT_SOURCE_DIR}/src/common
        # for Metal binding on Apple platforms
        ${PROJECT_SOURCE_DIR}/src/external/metal-cpp
        # for common_config.h:
        ${PROJECT_BINARY_DIR}/src
)

target_include_directories(
    conformance_test SYSTEM
    PRIVATE
        # for helper headers
        ${PROJECT_SOURCE_DIR}/external/include
        ${PROJECT_SOURCE_DIR}/src/external
        # Earcut algorithm for simple polygon triangulation
        ${PROJECT_SOURCE_DIR}/src/external/earcut/include
)
if(ANDROID)
    target_sources(
        conformance_test PRIVATE $<TARGET_OBJECTS:android_native_app_glue>
    )
endif()
if(WIN32)
    target_compile_definitions(conformance_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(
            conformance_test
            PRIVATE
                /Zc:wchar_t
                /Zc:forScope
                /W4
                /wd4996
        )
        if(NOT
           CMAKE_CXX_COMPILER_ID
           STREQUAL
           "Clang"
        )
            # If actually msvc and not clang-cl
            target_compile_options(conformance_test PRIVATE /WX)
        endif()
    endif()
endif()

if(APPLE)
    # use C++17 since there is a dependency on metal-cpp
    target_compile_features(conformance_test PUBLIC cxx_std_17)

    target_link_libraries(
        conformance_test PRIVATE "-framework Foundation"
                                 "-framework CoreGraphics" "-framework Metal"
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(conformance_test PRIVATE -Wall)
    target_link_libraries(conformance_test PRIVATE m)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GCC")
    target_compile_options(
        conformance_test PRIVATE -Wno-missing-field-initializers
    )
endif()
if(ANDROID)
    target_sources(
        conformance_test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../platform_specific/android_main.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../platform_specific/android_intent_extras.cpp"
            "${PROJECT_SOURCE_DIR}/src/external/jnipp/jnipp.cpp"
    )
    target_include_directories(
        conformance_test PRIVATE . "${PROJECT_SOURCE_DIR}/src/external"
    )
    target_link_libraries(
        conformance_test PRIVATE ${ANDROID_LIBRARY} ${ANDROID_LOG_LIBRARY}
    )
endif()

# Dynamic Library:
#  - Make build depend on the module definition/version script/export map
#  - Add the linker flag (except windows)
if(WIN32)
    target_sources(
        conformance_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/conformance_test.def"
    )
elseif(APPLE)
    set_target_properties(
        conformance_test
        PROPERTIES
            LINK_FLAGS
            "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/conformance_test.expsym"
    )
    target_sources(
        conformance_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/conformance_test.expsym"
    )
elseif(ANDROID)
    set_target_properties(
        conformance_test
        PROPERTIES
            LINK_FLAGS
            "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/conformance_test_android.map\""
    )
    target_sources(
        conformance_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/conformance_test_android.map"
    )
else()
    set_target_properties(
        conformance_test
        PROPERTIES
            LINK_FLAGS
            "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/conformance_test.map\""
    )
    target_sources(
        conformance_test
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/conformance_test.map"
    )
endif()

if(BUILD_CONFORMANCE_CLI)
    # Copy conformance_test assets files to the conformance_cli binary folder
    foreach(ASSET ${ASSETS})
        add_custom_command(
            TARGET conformance_test
            PRE_BUILD
            COMMAND
                ${CMAKE_COMMAND} -E copy ${ASSET}
                $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
        )
        install(FILES ${ASSET} DESTINATION conformance)
    endforeach()

    # Copy conformance test binary to conformance_cli binary folder
    add_custom_command(
        TARGET conformance_test
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy $<TARGET_FILE:conformance_test>
            $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
    )

    # Copy test layer files to conformance_cli binary folder
    add_custom_command(
        TARGET conformance_test
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_conformance_test_layer.json
            $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
    )
endif()

set_target_properties(
    conformance_test PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER}
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/XrApiLayer_conformance_test_layer.json
    DESTINATION conformance
)

install(
    TARGETS conformance_test
    LIBRARY DESTINATION conformance
    ARCHIVE DESTINATION conformance
    RUNTIME DESTINATION conformance
)
if(MSVC)
    install(
        FILES $<TARGET_PDB_FILE:conformance_test>
        DESTINATION conformance
        OPTIONAL
    )
endif()
