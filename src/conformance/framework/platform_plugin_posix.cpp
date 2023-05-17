// Copyright (c) 2019-2023, The Khronos Group Inc.
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

#include "platform_plugin.h"

#if defined(XR_OS_APPLE) || defined(XR_OS_LINUX)

namespace Conformance
{

    class PlatformPluginPosix : public IPlatformPlugin
    {
    public:
        PlatformPluginPosix() = default;

        ~PlatformPluginPosix() override
        {
            Shutdown();
        }

        bool Initialize() override
        {
            initialized = true;
            return initialized;
        }

        bool IsInitialized() const override
        {
            return initialized;
        }

        void Shutdown() override
        {
            if (initialized) {
                initialized = false;
            }
        }

        std::string DescribePlatform() const override
        {
            return "Posix";
        }

        std::vector<std::string> GetInstanceExtensions() const override
        {
            return {};
        }

        XrBaseInStructure* PopulateNextFieldForStruct(XrStructureType t) const override
        {
            (void)t;
            return nullptr;
        }

    private:
        bool initialized;
    };

    std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin()
    {
        return std::make_shared<PlatformPluginPosix>();
    }

}  // namespace Conformance

#endif  // defined(XR_OS_APPLE) || defined(XR_OS_LINUX)
