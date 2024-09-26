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

add_library(
    conformance_framework_basisu STATIC
    ${PROJECT_SOURCE_DIR}/src/external/basis_universal/transcoder/basisu_transcoder.cpp
    ${PROJECT_SOURCE_DIR}/src/external/basis_universal/zstd/zstd.c
)

target_include_directories(
    conformance_framework_basisu
    PUBLIC ${PROJECT_SOURCE_DIR}/src/external/basis_universal
)
set_target_properties(
    conformance_framework_basisu PROPERTIES FOLDER ${CONFORMANCE_TESTS_FOLDER}
)
if(CMAKE_CXX_COMPILER_ID STREQUAL GNU OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"
                                          AND NOT MSVC)
)
    # GCC and Clang-based compilers, we know the "no strict aliasing" flag.
    target_compile_options(
        conformance_framework_basisu PRIVATE -fno-strict-aliasing
    )
endif()


# basis_universal needs a lot of warnings disabled to build cleanly, unfortunately.
# Some are event contagious, but we deal with that where we include, to avoid turning off
# warnings for the entire codebase.
include(CheckCCompilerFlag)
check_c_compiler_flag(-Wunused-value HAVE_UNUSED_VALUE)
check_c_compiler_flag(-Wunused-local-typedefs HAVE_UNUSED_LOCAL_TYPEDEFS)
check_c_compiler_flag(-Wunused-parameter HAVE_UNUSED_PARAMETER)
check_c_compiler_flag(-Wunused-variable HAVE_UNUSED_VARIABLE)
check_c_compiler_flag(-Wclass-memaccess HAVE_CLASS_MEMACCESS)
check_c_compiler_flag(-Wmaybe-uninitialized HAVE_MAYBE_UNINITIALIZED)
if(HAVE_UNUSED_VALUE)
    target_compile_options(
        conformance_framework_basisu PRIVATE -Wno-unused-value
    )
endif()
if(HAVE_UNUSED_LOCAL_TYPEDEFS)
    target_compile_options(
        conformance_framework_basisu PRIVATE -Wno-unused-local-typedefs
    )
endif()
if(HAVE_UNUSED_PARAMETER)
    target_compile_options(
        conformance_framework_basisu PRIVATE -Wno-unused-parameter
    )
endif()
if(HAVE_UNUSED_VARIABLE)
    target_compile_options(
        conformance_framework_basisu PRIVATE -Wno-unused-parameter
    )
endif()
if(HAVE_CLASS_MEMACCESS)
    target_compile_options(
        conformance_framework_basisu PRIVATE -Wno-class-memaccess
    )
endif()
if(HAVE_MAYBE_UNINITIALIZED)
    # This one is actually for zstd, and it's a false positive.
    target_compile_options(
        conformance_framework_basisu PRIVATE -Wno-maybe-uninitialized
    )
endif()

add_subdirectory(conformance_layer)
add_subdirectory(utilities)
add_subdirectory(framework)
add_subdirectory(conformance_test)
if(NOT ANDROID)
    add_subdirectory(conformance_cli)
endif()
