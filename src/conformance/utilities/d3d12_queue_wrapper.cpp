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

#if defined(XR_USE_GRAPHICS_API_D3D12)

#include "d3d12_queue_wrapper.h"

#include "throw_helpers.h"

#include <array>

namespace Conformance
{
    D3D12QueueWrapper::D3D12QueueWrapper(Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device, D3D12_COMMAND_LIST_TYPE type)
        : m_device(d3d12Device)
    {

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = type;
        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue),
                                                              reinterpret_cast<void**>(m_cmdQueue.ReleaseAndGetAddressOf())));

        XRC_CHECK_THROW_HRCMD(d3d12Device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                                       reinterpret_cast<void**>(m_fence.ReleaseAndGetAddressOf())));
        m_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
        XRC_CHECK_THROW(m_fenceEvent != nullptr);
    }

    D3D12QueueWrapper::~D3D12QueueWrapper()
    {
        CPUWaitOnFence();
        m_cmdQueue.Reset();
        m_fence.Reset();
        if (m_fenceEvent != INVALID_HANDLE_VALUE) {
            ::CloseHandle(m_fenceEvent);
            m_fenceEvent = INVALID_HANDLE_VALUE;
        }
    }

    bool D3D12QueueWrapper::ExecuteCommandList(ID3D12CommandList* commandList) const
    {

        bool success;
        // weird structured exception handling stuff that windows does.
        __try {
            std::array<ID3D12CommandList*, 1> cmdLists = {{commandList}};
            m_cmdQueue->ExecuteCommandLists((UINT)cmdLists.size(), cmdLists.data());
            success = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            success = false;
        }

        ++m_fenceValue;
        XRC_CHECK_THROW_HRCMD(m_cmdQueue->Signal(m_fence.Get(), m_fenceValue));
        m_cpuWaited = false;

        return success;
    }

    void D3D12QueueWrapper::CPUWaitOnFence()
    {
        if (m_cpuWaited) {
            return;
        }
        if (m_fence->GetCompletedValue() < m_fenceValue) {
            if (m_fenceEvent == INVALID_HANDLE_VALUE) {
                m_fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
            }
            m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
        m_cpuWaited = true;
    }

    void D3D12QueueWrapper::GPUWaitOnOtherFence(ID3D12Fence* otherFence, uint64_t otherFenceValue)
    {
        XRC_CHECK_THROW_HRCMD(m_cmdQueue->Wait(otherFence, otherFenceValue));
    }

}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_D3D12
