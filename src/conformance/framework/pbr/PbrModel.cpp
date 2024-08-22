// Copyright 2022-2024, The Khronos Group Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#include "PbrModel.h"

#include "PbrCommon.h"

#include "common/xr_linear.h"

#include <stdexcept>

namespace Pbr
{
    Model::Model()
    {
        constexpr XrMatrix4x4f identityMatrix = Matrix::Identity;
        AddNode(identityMatrix, RootParentNodeIndex, "root");
    }

    NodeIndex_t Model::AddNode(const XrMatrix4x4f& transform, Pbr::NodeIndex_t parentIndex, std::string name)
    {
        auto newNodeIndex = (Pbr::NodeIndex_t)m_nodes.size();
        if (newNodeIndex != RootNodeIndex && parentIndex == RootParentNodeIndex) {
            throw std::runtime_error("Only the first node can be the root");
        }

        m_nodes.emplace_back(transform, std::move(name), newNodeIndex, parentIndex);
        return m_nodes.back().GetNodeIndex();
    }

    bool Model::FindFirstNode(NodeIndex_t* outNodeIndex, const char* name, const NodeIndex_t* parentNodeIndex) const
    {
        // Children are guaranteed to come after their parents, so start looking after the parent index if one is provided.
        const NodeIndex_t startIndex = parentNodeIndex ? *parentNodeIndex + 1 : Pbr::RootNodeIndex;
        for (NodeIndex_t i = startIndex; i < m_nodes.size(); ++i) {
            const Pbr::Node& node = m_nodes[i];
            if ((!parentNodeIndex || node.GetParentNodeIndex() == *parentNodeIndex) && (node.CompareName(name) == 0)) {
                *outNodeIndex = node.GetNodeIndex();
                return true;
            }
        }
        return false;
    }

    void Model::AddPrimitive(PrimitiveHandle primitive)
    {
        m_primitiveHandles.push_back(primitive);
    }

    Node::Node(Node&& other) noexcept
    {
        *this = std::move(other);
    }

    Node& Node::operator=(Node&& other) noexcept
    {
        if (&other == this) {
            return *this;
        }
        using std::swap;
        swap(Name, other.Name);
        swap(Index, other.Index);
        swap(ParentNodeIndex, other.ParentNodeIndex);
        swap(m_localTransform, other.m_localTransform);
        return *this;
    }

}  // namespace Pbr
