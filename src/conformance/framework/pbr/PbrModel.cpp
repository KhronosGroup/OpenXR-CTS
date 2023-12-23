// Copyright 2022-2023, The Khronos Group, Inc.
//
// Based in part on code that is:
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#include "PbrModel.h"

#include "PbrCommon.h"

#include "common/xr_linear.h"

#include <atomic>
#include <memory>
#include <stdexcept>

namespace Pbr
{
    Model::Model(bool createRootNode /*= true*/)
    {
        if (createRootNode) {
            XrMatrix4x4f identityMatrix;
            XrMatrix4x4f_CreateIdentity(&identityMatrix);
            AddNode(identityMatrix, RootParentNodeIndex, "root");
        }
    }

    NodeIndex_t Model::AddNode(const XrMatrix4x4f& transform, Pbr::NodeIndex_t parentIndex, std::string name)
    {
        auto newNodeIndex = (Pbr::NodeIndex_t)m_nodes.size();
        if (newNodeIndex != RootNodeIndex && parentIndex == RootParentNodeIndex) {
            throw std::runtime_error("Only the first node can be the root");
        }

        m_nodes.emplace_back(transform, std::move(name), newNodeIndex, parentIndex);
        // m_modelTransformsStructuredBuffer = nullptr;  // Structured buffer will need to be recreated.
        InvalidateBuffer();  // Structured buffer will need to be recreated.
        return m_nodes.back().Index;
    }

    void Model::Clear()
    {
        m_primitives.clear();
    }

    bool Model::FindFirstNode(NodeIndex_t* outNodeIndex, const char* name, const NodeIndex_t* parentNodeIndex) const
    {
        // Children are guaranteed to come after their parents, so start looking after the parent index if one is provided.
        const NodeIndex_t startIndex = parentNodeIndex ? *parentNodeIndex + 1 : Pbr::RootNodeIndex;
        for (NodeIndex_t i = startIndex; i < m_nodes.size(); ++i) {
            const Pbr::Node& node = m_nodes[i];
            if ((!parentNodeIndex || node.ParentNodeIndex == *parentNodeIndex) && (node.Name.compare(name) == 0)) {
                *outNodeIndex = node.Index;
                return true;
            }
        }
        return false;
    }

    XrMatrix4x4f Model::GetNodeToModelRootTransform(NodeIndex_t nodeIndex) const
    {
        const Pbr::Node& node = GetNode(nodeIndex);

        // Compute the transform recursively.
        XrMatrix4x4f identityMatrix;
        XrMatrix4x4f_CreateIdentity(&identityMatrix);
        const XrMatrix4x4f parentTransform =
            node.ParentNodeIndex == Pbr::RootNodeIndex ? identityMatrix : GetNodeToModelRootTransform(node.ParentNodeIndex);
        XrMatrix4x4f nodeTransform = node.GetTransform();
        XrMatrix4x4f result;
        XrMatrix4x4f_Multiply(&result, &nodeTransform, &parentTransform);
        return result;
    }

    void Model::AddPrimitive(PrimitiveHandle primitive)
    {
        m_primitives.push_back(primitive);
    }

    Node::Node(Node&& other) noexcept
    {
        using std::swap;
        swap(Name, other.Name);
        swap(Index, other.Index);
        swap(ParentNodeIndex, other.ParentNodeIndex);
        m_modifyCount.store(other.m_modifyCount);
        swap(m_localTransform, other.m_localTransform);
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
        m_modifyCount.store(other.m_modifyCount);
        swap(m_localTransform, other.m_localTransform);
        return *this;
    }

}  // namespace Pbr
