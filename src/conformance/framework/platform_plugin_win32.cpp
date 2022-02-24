// Copyright (c) 2019-2022, The Khronos Group Inc.
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

#ifdef XR_USE_PLATFORM_WIN32

#include <windows.h>

namespace Conformance
{

    class PlatformPluginWin32 : public IPlatformPlugin
    {
    public:
        PlatformPluginWin32() = default;

        ~PlatformPluginWin32()
        {
            Shutdown();
        }

        virtual bool Initialize() override
        {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            initialized = !FAILED(hr);
            return initialized;
        }

        virtual bool IsInitialized() const override
        {
            return initialized;
        }

        virtual void Shutdown() override
        {
            if (initialized) {
                CoUninitialize();
                initialized = false;
            }
        }

        virtual std::string DescribePlatform() const override
        {
            return "Windows";
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

    protected:
        bool initialized;
    };

    std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin()
    {
        return std::make_shared<PlatformPluginWin32>();
    }

}  // namespace Conformance

#endif  // XR_USE_PLATFORM_WIN32
