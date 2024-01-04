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

#include <map>
#include <memory>

#include "RuntimeFailure.h"
#include "IGraphicsValidator.h"

namespace Conformance
{

// Graphics Validator factories are forward declared here.
#ifdef XR_USE_GRAPHICS_API_D3D11
    std::shared_ptr<IGraphicsValidator> CreateGraphicsValidator_D3D11();
#endif

    using GraphicsValidatorFactory = std::function<std::shared_ptr<IGraphicsValidator>()>;

    const std::map<XrStructureType, GraphicsValidatorFactory> graphicsValidatorMap = {
#ifdef XR_USE_GRAPHICS_API_D3D11
        {XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, []() { return CreateGraphicsValidator_D3D11(); }},
#endif
    };

    std::shared_ptr<IGraphicsValidator> CreateGraphicsValidator(XrStructureType swapchainImageType) noexcept(false)
    {
        auto apiIt = graphicsValidatorMap.find(swapchainImageType);
        if (apiIt == graphicsValidatorMap.end()) {
            return std::shared_ptr<IGraphicsValidator>();
        }

        return apiIt->second();
    }

}  // namespace Conformance
