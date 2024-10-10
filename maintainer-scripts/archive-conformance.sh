#!/bin/sh
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

# archive-conformance.sh - Generate a tarball containing the repo subset
# for OpenXR-CTS
#
# Usage: ./archive-conformance.sh

set -e

(
# shellcheck disable=SC2086
SCRIPTS=$(cd "$(dirname $0)" && pwd)
# shellcheck disable=SC2086
cd "$(dirname $0)/.."
ROOT=$(pwd)
export ROOT

# shellcheck disable=SC1091
. "$SCRIPTS/common.sh"

TARNAME=OpenXR-CTS

# shellcheck disable=SC2046
makeSubset "$TARNAME" $(getConformanceFilenames)
(
    cd github

    # Add the shared public .mailmap used in all GitHub projects derived from the internal openxr repo
    add_to_tar "$TARNAME" .mailmap

    # Add the shared COPYING.adoc used in all GitHub projects derived from the internal openxr repo
    add_to_tar "$TARNAME" COPYING.adoc

    # Copy the README
    cp ../src/conformance/README.md README.md
    add_to_tar "$TARNAME" README.md
    rm README.md
)

echo
gzip_a_tar "$TARNAME"
)
