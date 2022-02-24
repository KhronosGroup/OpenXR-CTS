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

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <openxr/openxr.h>

namespace Conformance
{

    // IPlatformPlugin
    //
    // Wraps platform-specific implementation so the main openxr program can be platform-independent.
    //
    struct IPlatformPlugin
    {
        virtual ~IPlatformPlugin() = default;

        // Required before use of any member functions as described for each function.
        virtual bool Initialize() = 0;

        // Identifies if the PlatformPlugin has successfully initialized.
        // May be called regardless of initialization state.
        virtual bool IsInitialized() const = 0;

        // Matches Initialize.
        // May be called only if successfully initialized.
        virtual void Shutdown() = 0;

        // Returns a string describing the platform.
        // May be called regardless of initialization state.
        // Example returned string: "Windows"
        virtual std::string DescribePlatform() const = 0;

        // OpenXR instance-level extensions required by this platform.
        // Returns empty vector if there are no required extensions.
        // May be called only if successfully initialized.
        virtual std::vector<std::string> GetInstanceExtensions() const = 0;

        // Provide extension to the next field for the given structure.
        // Returns nullptr if there are no extension needed.
        // May be called only if successfully initialized.
        virtual XrBaseInStructure* PopulateNextFieldForStruct(XrStructureType t) const = 0;
    };

    // Create a platform plugin for the platform specified at compile time.
    // Always returns a valid IPlatformPlugin.
    // The interface must be successfully initialized by the caller before use.
    //
    // Example usage:
    //     std::shared_ptr<IPlatformPlugin> ipp = CreatePlatformPlugin();
    //     if(!ipp->Initialize())
    //         fail();
    //     [...]
    //     ipp->Shutdown();
    //
    std::shared_ptr<IPlatformPlugin> CreatePlatformPlugin();

}  // namespace Conformance
