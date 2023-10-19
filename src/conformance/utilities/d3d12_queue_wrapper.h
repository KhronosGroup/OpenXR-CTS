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

#include <d3d12.h>
#include <wrl/client.h>  // For Microsoft::WRL::ComPtr

#include <stdint.h>
#include <utility>

namespace Conformance
{
    /// Wraps a command queue, a fence, and the value last signaled for the fence
    class D3D12QueueWrapper
    {
    public:
        D3D12QueueWrapper(Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device, D3D12_COMMAND_LIST_TYPE type);
        ~D3D12QueueWrapper();

        /// Execute a command list, increment the fence value, and signal the fence.
        /// @return false on failure
        bool ExecuteCommandList(ID3D12CommandList* commandList) const;

        /// CPU wait on the most recently-signaled fence value
        void CPUWaitOnFence();

        /// GPU wait in this queue on some other fence
        void GPUWaitOnOtherFence(ID3D12Fence* otherFence, uint64_t otherFenceValue);

        void GPUWaitOnOtherFence(std::pair<ID3D12Fence*, uint64_t> otherFenceAndValue)
        {
            GPUWaitOnOtherFence(otherFenceAndValue.first, otherFenceAndValue.second);
        }

        /// Get the internal fence
        Microsoft::WRL::ComPtr<ID3D12Fence> GetFence() const
        {
            return m_fence;
        }

        /// Get the completed fence value (not the most recently signaled)
        uint64_t GetCompletedFenceValue() const
        {
            return m_fence->GetCompletedValue();
        }

        /// Get the most recently signaled fence value
        uint64_t GetSignaledFenceValue() const
        {
            return m_fenceValue;
        }

        /// Get the command queue for passing in to OpenXR or similar
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetCommandQueue() const
        {
            return m_cmdQueue;
        }

    private:
        Microsoft::WRL::ComPtr<ID3D12Device> m_device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_cmdQueue;
        Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
        mutable uint64_t m_fenceValue = 0;
        mutable bool m_cpuWaited = true;
        HANDLE m_fenceEvent = INVALID_HANDLE_VALUE;
    };
}  // namespace Conformance

#endif  // XR_USE_GRAPHICS_API_D3D12
