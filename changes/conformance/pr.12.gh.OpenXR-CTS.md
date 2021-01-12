Fix: Added `WaitForGpu` call at the end of `RenderView` in `D3D12GraphicsPlugin`. Without this the array and wide swapchain tests failed on some hardware/driver versions.
