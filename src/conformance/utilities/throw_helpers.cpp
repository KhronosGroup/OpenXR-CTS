// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "throw_helpers.h"
#include "stringification.h"
#include "utils.h"

namespace Conformance
{

    void ThrowXrResult(XrResult res, const char* originator, const char* sourceLocation) noexcept(false)
    {
        Throw(StringSprintf("XrResult failure [%s]", ResultToString(res)), originator, sourceLocation);
    }

    XrResult CheckThrowXrResultSuccessOrLimitReached(XrResult res, const char* originator, const char* sourceLocation) noexcept(false)
    {
        if (XR_FAILED(res) && res != XR_ERROR_LIMIT_REACHED) {
            Throw(StringSprintf("XrResult failure (and not XR_ERROR_LIMIT_REACHED) [%s]", ResultToString(res)), originator, sourceLocation);
        }
        return res;
    }

}  // namespace Conformance
