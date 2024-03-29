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

add_library(
    conformance_framework_mikktspace STATIC
    ${PROJECT_SOURCE_DIR}/src/external/mikktspace/mikktspace.c
)

target_include_directories(
    conformance_framework_mikktspace
    PUBLIC ${PROJECT_SOURCE_DIR}/src/external/mikktspace
)
set_target_properties(
    conformance_framework_mikktspace PROPERTIES FOLDER
                                                ${CONFORMANCE_TESTS_FOLDER}
)

add_subdirectory(conformance_layer)
add_subdirectory(utilities)
add_subdirectory(framework)
add_subdirectory(conformance_test)
if(NOT ANDROID)
    add_subdirectory(conformance_cli)
endif()
