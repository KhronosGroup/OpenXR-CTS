# Copyright (c) 2019-2023, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# tinygltf

add_library(conformance_framework_tinygltf STATIC gltf.cpp)

target_include_directories(
  conformance_framework_tinygltf
  # stb
  PRIVATE ${PROJECT_SOURCE_DIR}/src/external/stb
          # tinygltf impl needs bundled json.hpp
          ${PROJECT_SOURCE_DIR}/src/external/tinygltf
  # tinygltf itself
  PUBLIC ${PROJECT_SOURCE_DIR}/src/external)

target_compile_definitions(
  conformance_framework_tinygltf
  PUBLIC TINYGLTF_USE_CPP14 TINYGLTF_NO_STB_IMAGE_WRITE
  # TODO once we have rapidjson to avoid stack overflow
  # RAPIDJSON_PARSE_DEFAULT_FLAGS=kParseIterativeFlag TINYGLTF_USE_RAPIDJSON
  # TINYGLTF_USE_RAPIDJSON_CRTALLOCATOR
)

if(SUPPORTS_Werrorunusedparameter)
  target_compile_options(conformance_framework_tinygltf
                         PRIVATE -Wno-unused-parameter)
endif()
set_target_properties(conformance_framework_tinygltf
                      PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER})


# PBR subsystem and glTF handling
add_subdirectory(pbr)
add_subdirectory(gltf)

# Main conformance framework

set(VULKAN_SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_shaders/frag.glsl
                   ${CMAKE_CURRENT_SOURCE_DIR}/vulkan_shaders/vert.glsl)

run_xr_xml_generate(
  conformance_generator.py function_info.cpp
  ${PROJECT_SOURCE_DIR}/src/scripts/template_function_info.cpp)

add_library(
  conformance_framework STATIC
  action_utils.cpp
  catch_reporter_cts.cpp
  composition_utils.cpp
  conformance_framework.cpp
  conformance_utils.cpp
  controller_animation_handler.cpp
  environment.cpp
  graphics_plugin_d3d11.cpp
  graphics_plugin_d3d11_gltf.cpp
  graphics_plugin_d3d12.cpp
  graphics_plugin_d3d12_gltf.cpp
  graphics_plugin_factory.cpp
  graphics_plugin_opengl.cpp
  graphics_plugin_opengl_gltf.cpp
  graphics_plugin_opengles.cpp
  graphics_plugin_vulkan.cpp
  graphics_plugin_vulkan_gltf.cpp
  input_testinputdevice.cpp
  mesh_projection_layer.cpp
  platform_plugin_android.cpp
  platform_plugin_posix.cpp
  platform_plugin_win32.cpp
  report.cpp
  RGBAImage.cpp
  swapchain_image_data.cpp
  xml_test_environment.cpp
  ${VULKAN_SHADERS}
  ${CMAKE_CURRENT_BINARY_DIR}/function_info.cpp)

target_link_libraries(
  conformance_framework PUBLIC conformance_utilities OpenXR::openxr_loader
                               Threads::Threads Catch2 conformance_framework_gltf
                               conformance_framework_tinygltf)

if(GLSLANG_VALIDATOR AND NOT GLSL_COMPILER)
  target_compile_definitions(conformance_framework PUBLIC USE_GLSLANGVALIDATOR)
endif()

target_include_directories(
  conformance_framework
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
         # Strong types for integers, etc.
         ${PROJECT_SOURCE_DIR}/src/external/type-lite/include
         # Backport of std::span functionality to pre-C++17
         ${PROJECT_SOURCE_DIR}/src/external/span-lite/include
         # for xr_linear.h:
         ${PROJECT_SOURCE_DIR}/src/common
         # for openxr.h:
         ${PROJECT_BINARY_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
          # for compiled shaders
          ${CMAKE_CURRENT_BINARY_DIR}
          # for STB utilities, etc
          ${PROJECT_SOURCE_DIR}/src/external)

compile_glsl(run_conformance_test_glsl_compiles ${VULKAN_SHADERS})

add_dependencies(conformance_framework generate_openxr_header
                 run_conformance_test_glsl_compiles)

source_group("Vulkan Shaders" FILES ${VULKAN_SHADERS})

if(Vulkan_FOUND)
  target_include_directories(conformance_framework
                             PUBLIC ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(conformance_framework PUBLIC ${Vulkan_LIBRARY})
endif()

if(TARGET openxr-gfxwrapper)
  target_link_libraries(conformance_framework PUBLIC openxr-gfxwrapper)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GCC")
    target_compile_options(conformance_framework PUBLIC -Wno-missing-field-initializers)
endif()

if(WIN32)

  target_compile_definitions(conformance_framework
                             PUBLIC _CRT_SECURE_NO_WARNINGS)
  if(MSVC)
    target_compile_options(conformance_framework
                           PUBLIC /Zc:wchar_t /Zc:forScope /W4 /wd4996)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # If actually msvc and not clang-cl
        target_compile_options(conformance_framework PRIVATE /WX)
    endif()

    # Right now can't build this on MinGW because of directxcolors, directxmath, etc.
    target_link_libraries(conformance_framework PUBLIC d3d11 d3d12 d3dcompiler
                                                       dxgi)
  endif()
endif()
