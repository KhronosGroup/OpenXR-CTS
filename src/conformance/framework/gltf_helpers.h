// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

/// @file
/// Forward declare TinyGLTF types, and provide some helpers for loading a glTF file and asserting
/// that the load was successful.

#pragma once

#include <nonstd/span.hpp>

#include <memory>

namespace tinygltf
{
    class Model;
    class TinyGLTF;
}  // namespace tinygltf

namespace Conformance
{
    // Import a backported implementation of std::span, or std::span itself if available.

    using nonstd::span;

    /// Load a glTF file from memory into a shared pointer, throwing on errors.
    std::shared_ptr<tinygltf::Model> LoadGLTF(span<const uint8_t> data);

    /// Load a glTF file from memory into a shared pointer, throwing on errors, using the provided loader.
    std::shared_ptr<tinygltf::Model> LoadGLTF(span<const uint8_t> data, tinygltf::TinyGLTF& loader);

}  // namespace Conformance
