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

#ifdef XR_USE_PLATFORM_ANDROID

#include "conformance_framework.h"
#include "xr_dependencies.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <string>
#include <memory>
#include <vector>

namespace Conformance
{

    class PlatformPluginAndroid : public IPlatformPlugin
    {
    public:
        PlatformPluginAndroid() = default;

        ~PlatformPluginAndroid() override
        {
            Shutdown();
        }

        virtual bool Initialize() override
        {
            instanceCreateInfoAndroid = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
            instanceCreateInfoAndroid.applicationVM = Conformance_Android_Get_Application_VM();
            instanceCreateInfoAndroid.applicationActivity = Conformance_Android_Get_Application_Activity();

            initialized = true;
            return initialized;
        }

        virtual bool IsInitialized() const override
        {
            return initialized;
        }

        virtual void Shutdown() override
        {
            if (initialized) {
                initialized = false;
            }
        }

        virtual std::string DescribePlatform() const override
        {
            return "Android";
        }

        std::vector<std::string> GetInstanceExtensions() const override
        {
            return {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME};
        }

        XrBaseInStructure* PopulateNextFieldForStruct(XrStructureType t) const override
        {
            switch (t) {
            case XR_TYPE_INSTANCE_CREATE_INFO:
                return (XrBaseInStructure*)&instanceCreateInfoAndroid;

            default:
                return nullptr;
            }
            return nullptr;
        }

    protected:
        bool initialized;
        XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid;
    };

    std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin()
    {
        return std::make_shared<PlatformPluginAndroid>();
    }

}  // namespace Conformance

#endif  // XR_USE_PLATFORM_ANDROID
