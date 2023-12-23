# Copyright (c) 2020-2023, The Khronos Group Inc.
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

add_executable(conformance_cli ${LOCAL_SOURCE} ${LOCAL_HEADERS})

source_group("Headers" FILES ${LOCAL_HEADERS})

add_dependencies(conformance_cli conformance_test)

target_link_libraries(conformance_cli conformance_test)

target_include_directories(
    conformance_cli
    PRIVATE
        ../conformance_test
        ../framework
        ${PROJECT_SOURCE_DIR}/src/common
        ${PROJECT_SOURCE_DIR}/src/external
)

target_include_directories(
    conformance_cli
    PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/src/common
            ${PROJECT_SOURCE_DIR}/external/include
)

if(XR_USE_GRAPHICS_API_VULKAN)
    target_include_directories(conformance_cli PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

# Be able to find .so when installed
set_property(TARGET conformance_cli PROPERTY INSTALL_RPATH $ORIGIN)

set_target_properties(
    conformance_cli PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER}
)

install(TARGETS conformance_cli RUNTIME DESTINATION conformance)

if(MSVC)
    install(
        FILES $<TARGET_PDB_FILE:conformance_cli>
        DESTINATION conformance
        OPTIONAL
    )
endif()

# Install the README
install(FILES ../conformance_test/readme.md DESTINATION conformance)
