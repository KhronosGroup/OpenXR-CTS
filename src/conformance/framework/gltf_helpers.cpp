// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "gltf_helpers.h"
#include "report.h"
#include "gltf/GltfHelper.h"

#include "utilities/throw_helpers.h"
#include "cts_tinygltf.h"

namespace Conformance
{

    std::shared_ptr<const tinygltf::Model> LoadGLTF(span<const uint8_t> data)
    {
        tinygltf::TinyGLTF loader;

        return LoadGLTF(data, loader);
    }

    std::shared_ptr<const tinygltf::Model> LoadGLTF(span<const uint8_t> data, tinygltf::TinyGLTF& loader)
    {
        std::shared_ptr<tinygltf::Model> model = std::make_shared<tinygltf::Model>();
        std::string err;
        std::string warn;
        loader.SetImageLoader(GltfHelper::PassThroughKTX2, nullptr);
        bool loadedModel = loader.LoadBinaryFromMemory(model.get(), &err, &warn, data.data(), (unsigned int)data.size());
        if (!warn.empty()) {
            ReportF("glTF WARN: %s", &warn);
        }

        if (!err.empty()) {
            XRC_THROW("glTF ERR: " + err);
        }

        if (!loadedModel) {
            XRC_THROW("Failed to load glTF model provided.");
        }
        return std::const_pointer_cast<const tinygltf::Model>(std::move(model));
    }
}  // namespace Conformance
