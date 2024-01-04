# Copyright (c) 2019-2024, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

add_library(conformance_framework_gltf STATIC GltfHelper.cpp)

target_link_libraries(
    conformance_framework_gltf
    PUBLIC conformance_framework_tinygltf conformance_framework_pbr
    PRIVATE conformance_framework_mikktspace
)

target_include_directories(conformance_framework_gltf PUBLIC .)

set_target_properties(
    conformance_framework_gltf PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER}
)
