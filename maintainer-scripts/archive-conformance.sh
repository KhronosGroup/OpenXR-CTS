#!/bin/sh
# Copyright (c) 2019-2020 The Khronos Group Inc.
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
SCRIPTS=$(cd $(dirname $0) && pwd)
cd $(dirname $0)/..
ROOT=$(pwd)
export ROOT

# shellcheck source=common.sh
. $SCRIPTS/common.sh

CTS_TARNAME=OpenXR-CTS


makeSubset "$CTS_TARNAME" $(getConformanceFilenames)
(
    cd github
    # Add a symlink to README
    ln -s src/conformance/conformance_test/readme.md README.md
    add_to_tar "$CTS_TARNAME" README.md
    rm README.md
)

echo
gzip_a_tar "$CTS_TARNAME"
)
