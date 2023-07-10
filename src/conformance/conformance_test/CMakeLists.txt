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
#

file(GLOB LOCAL_HEADERS "*.h")
file(GLOB LOCAL_SOURCE "*.cpp")
file(GLOB ASSETS "composition_examples/*.png"
                 "SourceCodePro-Regular.otf")

# For including compiled shaders
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(conformance_test SHARED
    ${LOCAL_HEADERS}
    ${LOCAL_SOURCE}
    ${VULKAN_SHADERS}
    ${PROJECT_SOURCE_DIR}/src/common/platform_utils.hpp
)
if(ANDROID)
    target_sources(conformance_test PRIVATE $<TARGET_OBJECTS:android_native_app_glue>)
endif()

target_link_libraries(conformance_test PRIVATE conformance_utilities conformance_framework)

add_dependencies(conformance_test
    generate_openxr_header
    XrApiLayer_runtime_conformance
)
openxr_add_filesystem_utils(conformance_test)

target_include_directories(conformance_test
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../framework
    ${PROJECT_SOURCE_DIR}/src/common
    ${PROJECT_SOURCE_DIR}/src/external

    # Earcut algorithm for simple polygon triangulation
    ${PROJECT_SOURCE_DIR}/src/external/earcut/include

    # for openxr.h:
    ${PROJECT_BINARY_DIR}/include

    # for common_config.h:
    ${PROJECT_BINARY_DIR}/src

    # for helper headers
    ${PROJECT_SOURCE_DIR}/external/include
)

source_group("Header Files" FILES ${LOCAL_HEADERS})
source_group("Source Files" FILES ${LOCAL_SOURCE})

target_link_libraries(conformance_test PRIVATE conformance_framework)

if(WIN32)
    target_compile_definitions(conformance_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(conformance_test PRIVATE /Zc:wchar_t /Zc:forScope /W4 /WX /wd4996)
    endif()

endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(conformance_test PRIVATE -Wall)
    target_link_libraries(conformance_test PRIVATE m)
endif()
if(ANDROID)
    target_sources(
        conformance_test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../platform_specific/android_main.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../platform_specific/android_intent_extras.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../external/jnipp/jnipp.cpp"
    )
    target_include_directories(
        conformance_test
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../external")
    target_link_libraries(
        conformance_test
        PRIVATE
        ${ANDROID_LIBRARY}
        ${ANDROID_LOG_LIBRARY}
    )
endif()


if(BUILD_CONFORMANCE_CLI)
    # Copy conformance_test assets files to the conformance_cli binary folder
    foreach(ASSET ${ASSETS})
        add_custom_command(TARGET conformance_test PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${ASSET} $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
        )
        install(FILES ${ASSET}
            DESTINATION conformance)
    endforeach()

    # Copy conformance test binary to conformance_cli binary folder
    add_custom_command(TARGET conformance_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:conformance_test> $<TARGET_PROPERTY:conformance_cli,BINARY_DIR>
    )
endif()

set_target_properties(conformance_test PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER})

install(
    TARGETS conformance_test
    LIBRARY DESTINATION conformance
    ARCHIVE DESTINATION conformance
    RUNTIME DESTINATION conformance
)
if(MSVC)
    install(FILES $<TARGET_PDB_FILE:conformance_test> DESTINATION conformance OPTIONAL)
endif()
