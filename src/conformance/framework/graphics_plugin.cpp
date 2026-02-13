// Copyright (c) 2019-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "graphics_plugin.h"
#include <memory>

namespace Conformance
{

    void GlobalGraphicsPluginShutdownDevice()
    {
        GlobalData& globalData = GetGlobalData();
        if (globalData.IsUsingGraphicsPlugin()) {
            auto graphicsPlugin = globalData.GetGraphicsPlugin();
            graphicsPlugin->ShutdownDevice();
        }
    }

    GraphicsPluginShutdownDeviceOnScopeExit GraphicsPluginShutdownDeviceOnScopeExit::FromGlobalData()
    {
        GraphicsPluginShutdownDeviceOnScopeExit ret;
        GlobalData& globalData = GetGlobalData();
        if (globalData.IsUsingGraphicsPlugin()) {
            ret.m_plugin = globalData.GetGraphicsPlugin().get();
        }
        return ret;
    }

    GraphicsPluginShutdownDeviceOnScopeExit::GraphicsPluginShutdownDeviceOnScopeExit(
        GraphicsPluginShutdownDeviceOnScopeExit&& other) noexcept
    {
        using std::swap;
        swap(m_plugin, other.m_plugin);
    }

    GraphicsPluginShutdownDeviceOnScopeExit&
    GraphicsPluginShutdownDeviceOnScopeExit::operator=(GraphicsPluginShutdownDeviceOnScopeExit&& other) noexcept
    {
        if (&other == this) {
            // self assign
            return *this;
        }
        ShutdownDevice();
        using std::swap;
        swap(m_plugin, other.m_plugin);
        return *this;
    }

    GraphicsPluginShutdownDeviceOnScopeExit::~GraphicsPluginShutdownDeviceOnScopeExit()
    {
        ShutdownDevice();
    }

    void GraphicsPluginShutdownDeviceOnScopeExit::ShutdownDevice()
    {

        if (m_plugin == nullptr) {
            return;
        }

        m_plugin->ShutdownDevice();
        m_plugin = nullptr;
    }

    void GraphicsPluginShutdownDeviceOnScopeExit::Release()
    {
        m_plugin = nullptr;
    }

}  // namespace Conformance
