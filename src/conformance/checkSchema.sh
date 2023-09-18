#!/bin/sh -e
#
# Copyright (c) 2019-2023, The Khronos Group Inc.
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

# checkXml.sh - Check the integrity of the RELAX-NG Compact schema,
# then validate the registry XML against it. For full functionality on
# Debian and friends, first install the packages as follows:
#
#   sudo apt install trang jing libxml2-utils xmlstarlet
#
# Usage: checkXml.sh

RNC=ctsxml.rnc
RNG=ctsxml.rng
REGEN_RNC=ctsxml.regenerated.rnc

if which trang > /dev/null; then
    HAVE_TRANG=true
else
    HAVE_TRANG=false
fi

if $HAVE_TRANG; then
    echo
    echo "Converting $RNC to $RNG"
    trang $RNC $RNG
    echo "Converting $RNG back into $REGEN_RNC (formatted, but with some missing blank lines)"
    trang -o indent=4 $RNG $REGEN_RNC
    # Remove trailing whitespace from regenerated RNC
    sed -i 's/ *$//' $REGEN_RNC
else
    echo "Recommend installing 'trang' for schema syntax checking and rnc <-> rng conversions."
fi
