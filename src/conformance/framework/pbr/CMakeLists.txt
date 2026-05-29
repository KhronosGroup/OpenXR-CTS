# Copyright (c) 2019-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

add_library(
    conformance_framework_pbr STATIC
    GltfLoader.cpp
    PbrCommon.cpp
    PbrD3DCommon.cpp
    PbrMaterial.cpp
    PbrModel.cpp
    PbrSharedState.cpp
    PbrTexture.cpp
)
add_sanitizers(conformance_framework_pbr)

if(APPLE)
    # use C++17 since there is a dependency on metal-cpp
    target_compile_features(conformance_framework_pbr PUBLIC cxx_std_17)
endif()

set_target_properties(
    conformance_framework_pbr PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER}
)

target_link_libraries(
    conformance_framework_pbr
    PUBLIC conformance_framework_tinygltf conformance_framework_gltf
           conformance_utilities
)

if(MSVC)
    # Turns off ABI compatibility warning
    target_compile_definitions(
        conformance_framework_pbr PUBLIC _ENABLE_EXTENDED_ALIGNED_STORAGE
    )
endif()

if(XR_USE_GRAPHICS_API_D3D11 OR XR_USE_GRAPHICS_API_D3D12)
    include(fxc_shader)
    fxc_shader(
        INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrPixelShader.hlsl"
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/PbrPixelShader_hlsl.h"
        PROFILE
        ps_5_0
        VARIABLE
        g_PbrPixelShader
        EXTRA_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Shared.hlsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrShared.hlsl"
    )

    fxc_shader(
        INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrVertexShader.hlsl"
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/PbrVertexShader_hlsl.h"
        PROFILE
        vs_5_0
        VARIABLE
        g_PbrVertexShader
        EXTRA_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Shared.hlsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrShared.hlsl"
    )

    fxc_shader(
        INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrPixelShader.hlsl"
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/UnlitPixelShader_hlsl.h"
        PROFILE
        ps_5_0
        DEFINES
        "UNLIT"
        VARIABLE
        g_UnlitPixelShader
        EXTRA_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Shared.hlsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrShared.hlsl"
    )

    fxc_shader(
        INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrVertexShader.hlsl"
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/UnlitVertexShader_hlsl.h"
        PROFILE
        vs_5_0
        DEFINES
        "UNLIT"
        VARIABLE
        g_UnlitVertexShader
        EXTRA_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/Shared.hlsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrShared.hlsl"
    )

    target_sources(
        conformance_framework_pbr
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/PbrPixelShader_hlsl.h"
                "${CMAKE_CURRENT_BINARY_DIR}/PbrVertexShader_hlsl.h"
                "${CMAKE_CURRENT_BINARY_DIR}/UnlitPixelShader_hlsl.h"
                "${CMAKE_CURRENT_BINARY_DIR}/UnlitVertexShader_hlsl.h"
    )
endif()

if(XR_USE_GRAPHICS_API_D3D11)
    target_sources(
        conformance_framework_pbr
        PRIVATE
            D3D11/D3D11Resources.cpp
            D3D11/D3D11Texture.cpp
            D3D11/D3D11TextureCache.cpp
            D3D11/D3D11Material.cpp
            D3D11/D3D11Model.cpp
            D3D11/D3D11Primitive.cpp
    )
endif()

if(XR_USE_GRAPHICS_API_D3D12)
    target_sources(
        conformance_framework_pbr
        PRIVATE
            D3D12/D3D12PipelineStates.cpp
            D3D12/D3D12Resources.cpp
            D3D12/D3D12Texture.cpp
            D3D12/D3D12TextureCache.cpp
            D3D12/D3D12Material.cpp
            D3D12/D3D12Model.cpp
            D3D12/D3D12Primitive.cpp
    )

    target_include_directories(
        conformance_framework_pbr
        PRIVATE "${PROJECT_SOURCE_DIR}/src/external/d3dx12"
    )
endif()

if(XR_USE_GRAPHICS_API_OPENGL OR XR_USE_GRAPHICS_API_OPENGL_ES)
    include(make_includable)

    set(version_directive "^(#version [0-9]+)")
    set(defines_location "// DEFINES REPLACEMENT LOCATION //")
    make_includable(
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrVertexShader_glsl.vert"
        "${CMAKE_CURRENT_BINARY_DIR}/PbrVertexShader_glsl_src.h"
    )
    make_includable(
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrPixelShader_glsl.frag"
        "${CMAKE_CURRENT_BINARY_DIR}/PbrPixelShader_glsl_src.h"
    )
    make_includable(
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrVertexShader_glsl.vert"
        "${CMAKE_CURRENT_BINARY_DIR}/UnlitVertexShader_glsl_src.h"
        REPLACE
        "${defines_location}"
        "#define UNLIT"
    )
    make_includable(
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrPixelShader_glsl.frag"
        "${CMAKE_CURRENT_BINARY_DIR}/UnlitPixelShader_glsl_src.h"
        REPLACE
        "${defines_location}"
        "#define UNLIT"
    )

    set(gles_version_directive "#version 320 es")
    make_includable(
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrVertexShader_glsl.vert"
        "${CMAKE_CURRENT_BINARY_DIR}/PbrVertexShader_glsl_src_es.h"
        REPLACE
        "${version_directive}"
        "${gles_version_directive}"
    )
    make_includable(
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrPixelShader_glsl.frag"
        "${CMAKE_CURRENT_BINARY_DIR}/PbrPixelShader_glsl_src_es.h"
        REPLACE
        "${version_directive}"
        "${gles_version_directive}"
    )
    make_includable(
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrVertexShader_glsl.vert"
        "${CMAKE_CURRENT_BINARY_DIR}/UnlitVertexShader_glsl_src_es.h"
        REPLACE
        "${version_directive}"
        "${gles_version_directive}"
        "${defines_location}"
        "#define UNLIT"
    )
    make_includable(
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrPixelShader_glsl.frag"
        "${CMAKE_CURRENT_BINARY_DIR}/UnlitPixelShader_glsl_src_es.h"
        REPLACE
        "${version_directive}"
        "${gles_version_directive}"
        "${defines_location}"
        "#define UNLIT"
    )

    target_sources(
        conformance_framework_pbr
        PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/PbrPixelShader_glsl_src.h"
            "${CMAKE_CURRENT_BINARY_DIR}/PbrVertexShader_glsl_src.h"
            OpenGL/GLResources.cpp
            OpenGL/GLFormats.cpp
            OpenGL/GLTexture.cpp
            OpenGL/GLTextureCache.cpp
            OpenGL/GLMaterial.cpp
            OpenGL/GLModel.cpp
            OpenGL/GLPrimitive.cpp
    )
endif()

if(TARGET openxr-gfxwrapper)
    target_link_libraries(conformance_framework_pbr PRIVATE openxr-gfxwrapper)
endif()

if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(
        conformance_framework_pbr PRIVATE ${Vulkan_INCLUDE_DIRS}
    )
    target_link_libraries(conformance_framework_pbr PRIVATE ${Vulkan_LIBRARY})
endif()

if(XR_USE_GRAPHICS_API_VULKAN)
    include(glsl_shader)

    glsl_spv_shader(
        INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrVertexShader_glsl.vert"
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/PbrVertexShader_glsl_spv.h"
        STAGE
        vert
        VARIABLE
        g_PbrVertexShader_vulkan
        TARGET_ENV
        vulkan1.0
    )
    glsl_spv_shader(
        INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrPixelShader_glsl.frag"
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/PbrPixelShader_glsl_spv.h"
        STAGE
        frag
        VARIABLE
        g_PbrPixelShader_vulkan
        TARGET_ENV
        vulkan1.0
    )
    glsl_spv_shader(
        INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrVertexShader_glsl.vert"
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/UnlitVertexShader_glsl_spv.h"
        STAGE
        vert
        DEFINES
        "UNLIT"
        VARIABLE
        g_UnlitVertexShader_vulkan
        TARGET_ENV
        vulkan1.0
    )
    glsl_spv_shader(
        INPUT
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrPixelShader_glsl.frag"
        OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/UnlitPixelShader_glsl_spv.h"
        STAGE
        frag
        DEFINES
        "UNLIT"
        VARIABLE
        g_UnlitPixelShader_vulkan
        TARGET_ENV
        vulkan1.0
    )

    if(GLSLANG_VALIDATOR AND NOT GLSL_COMPILER)
        target_compile_definitions(
            conformance_framework_pbr PRIVATE USE_GLSLANGVALIDATOR
        )
    endif()

    target_sources(
        conformance_framework_pbr
        PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/PbrPixelShader_glsl_spv.h"
            "${CMAKE_CURRENT_BINARY_DIR}/PbrVertexShader_glsl_spv.h"
            "${CMAKE_CURRENT_BINARY_DIR}/UnlitPixelShader_glsl_spv.h"
            "${CMAKE_CURRENT_BINARY_DIR}/UnlitVertexShader_glsl_spv.h"
            Vulkan/VkPipelineStates.cpp
            Vulkan/VkResources.cpp
            Vulkan/VkFormats.cpp
            Vulkan/VkTexture.cpp
            Vulkan/VkTextureCache.cpp
            Vulkan/VkMaterial.cpp
            Vulkan/VkModel.cpp
            Vulkan/VkPrimitive.cpp
    )
    if(GLSLANG_VALIDATOR)
        # If we can, run this check of shader structure offsets in the unit test suite.
        add_test(
            NAME pbr_glsl_offsets
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/check-offsets.py"
                "${GLSLANG_VALIDATOR}"
        )
    endif()
endif()

if(XR_USE_GRAPHICS_API_METAL)

    target_sources(
        conformance_framework_pbr
        PRIVATE
            Metal/MetalMaterial.cpp
            Metal/MetalModel.cpp
            Metal/MetalPrimitive.cpp
            Metal/MetalResources.cpp
            Metal/MetalFormats.cpp
            Metal/MetalTexture.cpp
            Metal/MetalTexture.cpp
            Metal/MetalTextureCache.cpp
            Metal/MetalPipelineStates.cpp
            # headers
            Metal/MetalMaterial.h
            Metal/MetalModel.h
            Metal/MetalPrimitive.h
            Metal/MetalResources.h
            Metal/MetalTexture.h
            Metal/MetalTextureCache.h
            Metal/MetalPipelineStates.h
            # shaders
            Shaders/PbrShader.metal
    )
    source_group("Shaders" FILES Shaders/PbrShader.metal)

    # Create Metal library, with optional debug information
    function(metal_shader)
        set(options)
        set(oneValueArgs INPUT IR OUTPUT)
        set(multiValueArgs DEFINES)
        cmake_parse_arguments(_metal "${options}" "${oneValueArgs}"
                            "${multiValueArgs}" ${ARGN})

        set(defineArgList)
        foreach(define IN LISTS _metal_DEFINES)
            list(APPEND defineArgList "-D${define}")
        endforeach()

        add_custom_command(
            DEPENDS "${_metal_INPUT}"
            OUTPUT "${_metal_IR}"
            COMMAND
                "${MetalTools_METAL_EXECUTABLE}" #
                ARGS "$<$<CONFIG:Debug>:-gline-tables-only>"
                "$<$<CONFIG:Debug>:-frecord-sources>" #
                ${defineArgList} #
                -c "${_metal_INPUT}" -o "${_metal_IR}"
        )
        add_custom_command(
            DEPENDS "${_metal_IR}"
            OUTPUT "${_metal_OUTPUT}"
            COMMAND
                "${MetalTools_METALLIB_EXECUTABLE}" #
                ARGS "${_metal_IR}" -o "${_metal_OUTPUT}"
        )
    endfunction()

    metal_shader(
        INPUT "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrShader.metal"
        IR "${CMAKE_CURRENT_BINARY_DIR}/PbrShader.air"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/PbrShader.metallib")
    target_sources(conformance_framework_pbr PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/PbrShader.metallib")

    metal_shader(
        INPUT "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/PbrShader.metal"
        IR "${CMAKE_CURRENT_BINARY_DIR}/UnlitShader.air"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/UnlitShader.metallib"
        DEFINES "UNLIT")
    target_sources(conformance_framework_pbr PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/UnlitShader.metallib")

    set_source_files_properties(
        Shaders/PbrShader.metal PROPERTIES HEADER_FILE_ONLY TRUE
    )

    target_include_directories(
        conformance_framework_pbr
        PRIVATE "${PROJECT_SOURCE_DIR}/src/external/metal-cpp"
    )
endif()

target_include_directories(
    conformance_framework_pbr
    PUBLIC
        .
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/src/common"
        "${PROJECT_SOURCE_DIR}/src/conformance"
        # Strong types for integers, etc.
        "${PROJECT_SOURCE_DIR}/src/external/type-lite/include"
        # Backport span
        "${PROJECT_SOURCE_DIR}/src/external/span-lite/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src/external/stb"
)
