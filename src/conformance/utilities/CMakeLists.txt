# Copyright (c) 2019-2023, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

add_library(
  conformance_utilities STATIC
  bitmask_generator.cpp
  bitmask_to_string.cpp
  d3d_common.cpp
  event_reader.cpp
  Geometry.cpp
  stringification.cpp
  types_and_constants.cpp
  utils.cpp)

target_include_directories(
  conformance_utilities
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
         # Backport of std::span functionality to pre-C++17
         ${PROJECT_SOURCE_DIR}/src/external/span-lite/include
         # for openxr.h:
         ${PROJECT_BINARY_DIR}/include)

add_dependencies(conformance_utilities generate_openxr_header)

if(GLSLANG_VALIDATOR AND NOT GLSLC_COMMAND)
  target_compile_definitions(conformance_utilities PUBLIC USE_GLSLANGVALIDATOR)
endif()

if(Vulkan_FOUND)
  target_include_directories(conformance_utilities
                             PUBLIC ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(conformance_utilities PUBLIC ${Vulkan_LIBRARY})
endif()
