// Copyright 2022-2024, The Khronos Group Inc.
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
#include "utilities/xr_math_operators.h"

#include <nonstd/span.hpp>

#include <memory>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace Pbr
{
    using namespace openxr::math_operators;

    enum class NodeVisibility
    {
        Invisible,
        Visible,
        Inherit,
    };

    // Node for creating a hierarchy of transforms. These transforms are referenced by vertices in the model's primitives.
    struct Node
    {
        using Collection = std::vector<Node>;

        Node(const XrMatrix4x4f& localTransform, std::string name, NodeIndex_t index, NodeIndex_t parentNodeIndex)
            : Name(std::move(name)), Index(index), ParentNodeIndex(parentNodeIndex), m_localTransform(localTransform)
        {
        }

        Node(Node&& other) noexcept;
        Node& operator=(Node&& other) noexcept;

        // Compare this node's name to a given name.
        int CompareName(const char* value) const
        {
            return Name.compare(value);
        }

        // Get the local transform for this node.
        const XrMatrix4x4f& GetLocalTransform() const
        {
            return m_localTransform;
        }

        // Get the index of this node.
        NodeIndex_t GetNodeIndex() const
        {
            return Index;
        }

        // Get the index of the parent node of this node.
        NodeIndex_t GetParentNodeIndex() const
        {
            return ParentNodeIndex;
        }

    private:
        // All immutable, but we need copy-assign for vector
        std::string Name;
        NodeIndex_t Index;
        NodeIndex_t ParentNodeIndex;
        XrMatrix4x4f m_localTransform;
    };

    /// A model is a collection of primitives (which reference a material) and transforms referenced by the primitives' vertices.
    class Model
    {
    public:
        Model();

        /// Add a node to the model.
        NodeIndex_t AddNode(const XrMatrix4x4f& transform, NodeIndex_t parentIndex, std::string name = "");

        /// Add a primitive to the model.
        void AddPrimitive(PrimitiveHandle primitive);

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

        /// Get the number of primitives used in this model
        uint32_t GetPrimitiveCount() const
        {
            return (uint32_t)m_primitiveHandles.size();
        }

        /// Get a primitive handle by index of primitives used in this model.
        PrimitiveHandle GetPrimitiveHandle(uint32_t index) const
        {
            return m_primitiveHandles[index];
        }

        /// Find the first node (after an optional parent node) which matches a given name.
        bool FindFirstNode(NodeIndex_t* outNodeIndex, const char* name, const NodeIndex_t* parentNodeIndex = nullptr) const;

        const std::vector<PrimitiveHandle>& GetPrimitiveHandles() const
        {
            return m_primitiveHandles;
        }

        const Node::Collection& GetNodes() const
        {
            return m_nodes;
        }
        static constexpr Pbr::NodeIndex_t RootParentNodeIndex = (Pbr::NodeIndex_t)-1;

    private:
        // A model is made up of one or more Primitives. Each Primitive has a unique material.
        // Ideally primitives with the same material should be merged to reduce draw calls.
        std::vector<PrimitiveHandle> m_primitiveHandles;

        // A model contains one or more nodes. Each vertex of a primitive references a node to have the
        // node's transform applied.
        Node::Collection m_nodes;
    };

    /// A model instance is a collection of node transforms for an instance of a model.
    /// A model instance can only have its transforms updated once per command queue.
    /// A model instance holds a strong shared reference to its corresponding model.
    class ModelInstance
    {
    protected:
        explicit ModelInstance(std::shared_ptr<const Model> model) : m_model(std::move(model))
        {
            const auto nodeCount = m_model->GetNodeCount();

            m_nodeLocalVisibilities.resize(nodeCount, NodeVisibility::Inherit);
            m_resolvedVisibilities.resize(nodeCount, true);

            m_nodeLocalTransforms.reserve(m_model->GetNodeCount());
            for (const Node& node : m_model->GetNodes()) {
                m_nodeLocalTransforms.push_back(node.GetLocalTransform());
            }
            constexpr XrMatrix4x4f identityMatrix = Matrix::Identity;  // or better yet poison it
            m_resolvedTransforms.resize(nodeCount, identityMatrix);
        }

    public:
        /// Sets the visibility of a node. Nodes otherwise inherit
        void SetNodeVisibility(NodeIndex_t nodeIndex, NodeVisibility visibility)
        {
            m_nodeLocalVisibilities[nodeIndex] = visibility;
            // Visibility is implemented by scaling to 0
            m_resolvedTransformsNeedUpdate = true;
        }

        /// Overrides the local transform of a node
        void SetNodeTransform(NodeIndex_t nodeIndex, const XrMatrix4x4f& transform)
        {
            m_nodeLocalTransforms[nodeIndex] = transform;
            m_resolvedTransformsNeedUpdate = true;
        }

        /// Combine a transform with the original transform from the asset
        void SetAdditionalNodeTransform(NodeIndex_t nodeIndex, const XrMatrix4x4f& transform)
        {
            // Node transform is the immutable original transform
            const XrMatrix4x4f& originalNodeTransform = m_model->GetNode(nodeIndex).GetLocalTransform();
            XrMatrix4x4f compositeTransform = originalNodeTransform * transform;
            SetNodeTransform(nodeIndex, compositeTransform);
        }

    protected:
        bool ResolvedTransformsNeedUpdate() const noexcept
        {
            return m_resolvedTransformsNeedUpdate;
        }
        void MarkResolvedTransformsUpdated() noexcept
        {
            m_resolvedTransformsNeedUpdate = false;
        }
        void ResolveTransformsAndVisibilities(bool transpose)
        {
            const auto& nodes = m_model->GetNodes();

            // Nodes are guaranteed to come after their parents, so each node transform can be multiplied by its parent transform in a single pass.
            assert(nodes.size() == m_nodeLocalTransforms.size());
            assert(nodes.size() == m_resolvedTransforms.size());
            constexpr XrMatrix4x4f identityMatrix = Matrix::Identity;
            for (const auto& node : nodes) {
                bool parentIsRoot = node.GetParentNodeIndex() == Model::RootParentNodeIndex;
                assert(parentIsRoot || node.GetParentNodeIndex() < node.GetNodeIndex());

                bool parentVisibility = (parentIsRoot) ? true : m_resolvedVisibilities[node.GetParentNodeIndex()];
                NodeVisibility nodeVisibility = m_nodeLocalVisibilities[node.GetNodeIndex()];

                m_resolvedVisibilities[node.GetNodeIndex()] =
                    nodeVisibility == NodeVisibility::Inherit ? parentVisibility : nodeVisibility == NodeVisibility::Visible;

                const XrMatrix4x4f& parentTransform = (parentIsRoot) ? identityMatrix : m_resolvedTransforms[node.GetParentNodeIndex()];
                const XrMatrix4x4f& nodeTransform = m_nodeLocalTransforms[node.GetNodeIndex()];

                if (transpose) {
                    XrMatrix4x4f nodeTransformTranspose = Matrix::Transposed(nodeTransform);
                    m_resolvedTransforms[node.GetNodeIndex()] = nodeTransformTranspose * parentTransform;
                }
                else {
                    m_resolvedTransforms[node.GetNodeIndex()] = parentTransform * nodeTransform;
                }
            }

            // After all node transforms and visibilities have been propagated,
            // zero the transforms of invisible nodes.
            for (const auto& node : nodes) {
                if (!m_resolvedVisibilities[node.GetNodeIndex()]) {
                    XrMatrix4x4f_CreateScale(&m_resolvedTransforms[node.GetNodeIndex()], 0, 0, 0);
                }
            }
        }

        const Model& GetModel() const
        {
            return *m_model;
        }

        const std::vector<XrMatrix4x4f>& GetResolvedTransforms() const noexcept
        {
            return m_resolvedTransforms;
        }

        bool IsAnyNodeVisible(nonstd::span<const NodeIndex_t> nodeIndices) const
        {
            for (NodeIndex_t node : nodeIndices) {
                if (m_resolvedVisibilities[node]) {
                    return true;
                }
            }
            return false;
        }

    private:
        bool m_resolvedTransformsNeedUpdate{true};

        // Derived classes may depend on this being immutable.
        std::shared_ptr<const Model> m_model;
        std::vector<NodeVisibility> m_nodeLocalVisibilities;
        std::vector<bool> m_resolvedVisibilities;
        // This is initialized to the local transform of every node,
        // but can be updated for this instance.
        std::vector<XrMatrix4x4f> m_nodeLocalTransforms;
        std::vector<XrMatrix4x4f> m_resolvedTransforms;
    };
}  // namespace Pbr
