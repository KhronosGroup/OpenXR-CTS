# Copyright (c) 2019-2023, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

add_library(
    conformance_utilities STATIC
    bitmask_generator.cpp
    bitmask_to_string.cpp
    d3d_common.cpp
    d3d12_queue_wrapper.cpp
    d3d12_utils.cpp
    event_reader.cpp
    Geometry.cpp
    opengl_utils.cpp
    stringification.cpp
    swapchain_format_data.cpp
    throw_helpers.cpp
    types_and_constants.cpp
    utils.cpp
)

target_include_directories(
    conformance_utilities
    PUBLIC
        ..
        # Backport of std::span functionality to pre-C++17
        ${PROJECT_SOURCE_DIR}/src/external/span-lite/include
        # for GL:
        ${PROJECT_SOURCE_DIR}/external/include
        # for common/
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(conformance_utilities PUBLIC OpenXR::headers)

if(TARGET openxr-gfxwrapper)
    target_link_libraries(conformance_utilities PRIVATE openxr-gfxwrapper)
endif()

if(GLSLANG_VALIDATOR AND NOT GLSL_COMPILER)
    target_compile_definitions(
        conformance_utilities PUBLIC USE_GLSLANGVALIDATOR
    )
endif()

if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(
        conformance_utilities PUBLIC ${Vulkan_INCLUDE_DIRS}
    )
    target_link_libraries(conformance_utilities PUBLIC ${Vulkan_LIBRARY})
endif()

set_target_properties(
    conformance_utilities PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER}
)
