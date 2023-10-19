// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once

#include "PbrCommon.h"
#include "PbrHandles.h"

#include "common/xr_linear.h"

#include <atomic>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace Pbr
{
    // Node for creating a hierarchy of transforms. These transforms are referenced by vertices in the model's primitives.
    struct Node
    {
        using Collection = std::vector<Node>;

        Node(const XrMatrix4x4f& localTransform, std::string name, NodeIndex_t index, NodeIndex_t parentNodeIndex)
            : Name(std::move(name)), Index(index), ParentNodeIndex(parentNodeIndex)
        {
            SetTransform(localTransform);
        }

        Node(Node&& other) noexcept;
        Node& operator=(Node&& other) noexcept;

        // Set the local transform for this node.
        void SetTransform(const XrMatrix4x4f& transform)
        {
            m_localTransform = transform;
            ++m_modifyCount;
        }

        // Get the local transform for this node.
        XrMatrix4x4f GetTransform() const
        {
            return m_localTransform;
        }
        uint32_t GetModifyCount() const
        {
            return m_modifyCount;
        }

        std::string Name;
        NodeIndex_t Index;
        NodeIndex_t ParentNodeIndex;

    private:
        friend class Model;
        // TODO std::atomic_uint32_t
        std::atomic_uint32_t m_modifyCount{0};
        XrMatrix4x4f m_localTransform;
    };

    /// A model is a collection of primitives (which reference a material) and transforms referenced by the primitives' vertices.
    class Model
    {
    public:
        std::string Name;

        Model(bool createRootNode = true);

        // Add a node to the model.
        NodeIndex_t AddNode(const XrMatrix4x4f& transform, NodeIndex_t parentIndex, std::string name = "");

        // Add a primitive to the model.
        void AddPrimitive(PrimitiveHandle primitive);

        // Remove all primitives.
        void Clear();

        NodeIndex_t GetNodeCount() const
        {
            return (NodeIndex_t)m_nodes.size();
        }
        Node& GetNode(NodeIndex_t nodeIndex)
        {
            return m_nodes[nodeIndex];
        }
        const Node& GetNode(NodeIndex_t nodeIndex) const
        {
            return m_nodes[nodeIndex];
        }

        uint32_t GetPrimitiveCount() const
        {
            return (uint32_t)m_primitives.size();
        }
        PrimitiveHandle GetPrimitive(uint32_t index) const
        {
            return m_primitives[index];
        }

        // Find the first node which matches a given name.
        bool FindFirstNode(NodeIndex_t* outNodeIndex, const char* name, const NodeIndex_t* parentNodeIndex = nullptr) const;

    protected:
        // Invalidate buffers associated with model transforms
        bool m_modelTransformsStructuredBufferInvalid{true};
        void InvalidateBuffer()
        {
            m_modelTransformsStructuredBufferInvalid = true;
        }

        const std::vector<PrimitiveHandle>& GetPrimitives() const
        {
            return m_primitives;
        }

        const Node::Collection& GetNodes() const
        {
            return m_nodes;
        }
        static constexpr Pbr::NodeIndex_t RootParentNodeIndex = (Pbr::NodeIndex_t)-1;

    private:
        // Compute the transform relative to the root of the model for a given node.
        XrMatrix4x4f GetNodeToModelRootTransform(NodeIndex_t nodeIndex) const;

        // Updated the transforms used to render the model. This needs to be called any time a node transform is changed.
        // void UpdateTransforms(Pbr::D3D11Resources const& pbrResources, std::runtime_error ID3D11DeviceContext* context) const;

    private:
        // A model is made up of one or more Primitives. Each Primitive has a unique material.
        // Ideally primitives with the same material should be merged to reduce draw calls.
        std::vector<PrimitiveHandle> m_primitives;

        // A model contains one or more nodes. Each vertex of a primitive references a node to have the
        // node's transform applied.
        Node::Collection m_nodes;
    };
}  // namespace Pbr
