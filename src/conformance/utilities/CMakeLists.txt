# Copyright (c) 2019-2023, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

include(GetGitRevisionDescription)

# Pattern restricts naming to CTS tags in gitlab and github
git_describe_working_tree(CTS_GIT_REV --match "*cts-*")
git_get_exact_tag(CTS_EXACT_TAG_REV --match "*cts-*")
git_local_changes(CTS_GIT_DIRTY_STRING)

if(CTS_GIT_REV)
    set(CTS_GIT_SUCCEEDED TRUE)
    if(CTS_EXACT_TAG_REV)
        set(CTS_EXACT_TAG TRUE)
    else()
        message(
            STATUS
                "Not an exact release tag: conformance package must include diff(s) from the latest approved CTS release tag."
        )
        set(CTS_EXACT_TAG FALSE)
    endif()
else()
    message(
        STATUS
            "Could not describe git revision relative to a CTS release tag - do not use binaries for CTS submission"
    )
    # Try to get something at least.
    get_git_head_revision(CTS_REFSPEC CTS_GIT_HASH)
    set(CTS_GIT_REV "refspec:${CTS_REFSPEC} hash:${CTS_GIT_HASH}")
    set(CTS_GIT_SUCCEEDED FALSE)
endif()
if(CTS_GIT_DIRTY_STRING STREQUAL "DIRTY")
    set(CTS_GIT_DIRTY TRUE)
    message(STATUS "Git revision: ${CTS_GIT_REV} - local tree has changes!")
else()
    set(CTS_GIT_DIRTY FALSE)
    message(STATUS "Git revision: ${CTS_GIT_REV} - local tree clean")
endif()

configure_file(
    git_revision.cpp.in "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp"
)

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
    "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp"
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
