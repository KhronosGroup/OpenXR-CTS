# Copyright (c) 2019-2023, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

add_library(conformance_framework_gltf STATIC GltfHelper.cpp)

target_link_libraries(
  conformance_framework_gltf
  PUBLIC conformance_framework_tinygltf conformance_framework_pbr
  PRIVATE conformance_framework_mikktspace)

target_include_directories(conformance_framework_gltf
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(conformance_framework_gltf
                      PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER})
