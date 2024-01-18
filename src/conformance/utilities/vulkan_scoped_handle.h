// Copyright (c) 2019-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef XR_USE_GRAPHICS_API_VULKAN

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace Conformance
{
    /// A stateless destroyer for Vulkan handles that have a destroy function we can refer to statically.
    ///
    /// @tparam HandleType The handle type to wrap
    /// @tparam ParentHandle The parent handle type needed by the destroy function.
    /// @tparam DestroyFunction The function used to destroy the handle.
    ///
    /// @see ScopedVkWithDefaultDestroy
    /// @see ScopedVk
    /// @see VkDestroyerWithFuncPointer
    ///
    /// @ingroup cts_handle_helpers
    template <typename HandleType, typename ParentHandle,
              void(VKAPI_PTR* DestroyFunction)(ParentHandle, HandleType, const VkAllocationCallbacks*)>
    class VkDefaultDestroyer
    {
    public:
        void operator()(ParentHandle parent, HandleType handle) const noexcept
        {
            DestroyFunction(parent, handle, nullptr);
        }
    };

    /// A destroyer for Vulkan handles that holds state at runtime to contain a function pointer.
    ///
    /// This is mainly for things from extensions.
    ///
    /// @tparam HandleType The handle type to wrap
    /// @tparam ParentHandle The parent handle type needed by the destroy function.
    ///
    /// @see ScopedVkWithPfn
    /// @see VkDefaultDestroyer
    /// @see ScopedVk
    ///
    /// @ingroup cts_handle_helpers
    template <typename HandleType, typename ParentHandle>
    class VkDestroyerWithFuncPointer
    {
    public:
        using DestroyFunction = VKAPI_ATTR void (*)(ParentHandle, HandleType, const VkAllocationCallbacks*);

        VkDestroyerWithFuncPointer(DestroyFunction pfn) : pfn_(pfn)
        {
        }

        void operator()(ParentHandle parent, HandleType handle) const noexcept
        {
            pfn_(parent, handle, nullptr);
        }

    private:
        DestroyFunction pfn_;
    };

    /// A unique-ownership RAII helper for Vulkan handles.
    ///
    /// @tparam HandleType The handle type to wrap
    /// @tparam ParentHandle The parent handle type needed by the destroy function.
    /// @tparam Destroyer a functor type that destroys the handle - may be stateless or have state
    ///
    /// @see VkDefaultDestroyer
    /// @see VkDestroyerWithFuncPointer
    ///
    /// @ingroup cts_handle_helpers
    template <typename HandleType, typename ParentHandle, typename Destroyer>
    class ScopedVk
    {
    public:
        /// Default (empty) constructor
        ScopedVk() = default;

        /// Empty constructor when we need a destroyer instance.
        explicit ScopedVk(Destroyer d) : d_(d)
        {
        }

        /// Explicit constructor from handle and parent, if we don't need a destroyer instance.
        ///
        /// The parent handle is not owned, just observed.
        explicit ScopedVk(HandleType h, ParentHandle parent, std::enable_if<std::is_default_constructible<Destroyer>::value>* = nullptr)
            : h_(h), parent_(parent)
        {
        }

        /// Constructor from handle and parent when we need a destroyer instance.
        ///
        /// The parent handle is not owned, just observed.
        ScopedVk(HandleType h, ParentHandle parent, Destroyer d) : h_(h), parent_(parent), d_(d)
        {
        }

        /// Destructor
        ~ScopedVk()
        {
            reset();
        }

        /// Non-copyable
        ScopedVk(ScopedVk const&) = delete;

        /// Non-copy-assignable
        ScopedVk& operator=(ScopedVk const&) = delete;

        /// Move-constructible
        ScopedVk(ScopedVk&& other) noexcept : h_(std::move(other.h_)), parent_(std::move(other.parent_)), d_(std::move(other.d_))
        {
            other.clear();
        }

        /// Move-assignable
        ScopedVk& operator=(ScopedVk&& other) noexcept
        {
            if (&other == this) {
                return *this;
            }
            reset();
            swap(other);
            return *this;
        }

        /// Is this handle valid?
        constexpr bool valid() const noexcept
        {
            return get() != VK_NULL_HANDLE;
        }

        /// Is this handle valid?
        explicit operator bool() const noexcept
        {
            return valid();
        }

        void swap(ScopedVk& other) noexcept
        {
            std::swap(h_, other.h_);
            std::swap(parent_, other.parent_);
            std::swap(d_, other.d_);
        }

        /// Destroy the owned handle, if any.
        void reset()
        {
            if (get() != VK_NULL_HANDLE) {
                get_destroyer()(get_parent(), get());
                clear();
            }
        }

        /// Assign a new handle into this object's control, destroying the old one if applicable.
        /// The parent handle is not owned, just observed.
        void adopt(HandleType h, ParentHandle parent)
        {
            reset();
            h_ = h;
            parent_ = parent;
        }

        /// Assign a new handle into this object's control, including new destroyer, destroying the old one if applicable.
        /// The parent handle is not owned, just observed.
        void adopt(HandleType h, ParentHandle parent, Destroyer&& d)
        {
            adopt(h, parent);
            d_ = std::move(d);
        }

        /// Access the raw handle without affecting ownership or lifetime.
        HandleType get() const noexcept
        {
            return h_;
        }

        /// Access the raw handle of the parent
        ParentHandle get_parent() const noexcept
        {
            return parent_;
        }

        /// Access the destroyer functor
        const Destroyer& get_destroyer() const noexcept
        {
            return d_;
        }

        /// Release the handle from this object's control.
        HandleType release() noexcept
        {
            HandleType ret = h_;
            clear();
            return ret;
        }

    private:
        void clear() noexcept
        {
            h_ = VK_NULL_HANDLE;
            parent_ = VK_NULL_HANDLE;
        }
        HandleType h_ = VK_NULL_HANDLE;
        ParentHandle parent_ = VK_NULL_HANDLE;
        Destroyer d_;
    };

    /// Swap function for scoped handles, found using ADL.
    /// @relates ScopedVk
    template <typename HandleType, typename ParentHandle, typename Destroyer>
    inline void swap(ScopedVk<HandleType, ParentHandle, Destroyer>& a, ScopedVk<HandleType, ParentHandle, Destroyer>& b)
    {
        return a.swap(b);
    }

    /// Equality comparison between a scoped handle and a null handle
    /// @relates ScopedVk
    template <typename HandleType, typename ParentHandle, typename Destroyer>
    inline bool operator==(ScopedVk<HandleType, ParentHandle, Destroyer> const& handle, std::nullptr_t const&)
    {
        return !handle.valid();
    }

    /// Equality comparison between a scoped handle and a null handle
    /// @relates ScopedVk
    template <typename HandleType, typename ParentHandle, typename Destroyer>
    inline bool operator==(std::nullptr_t const&, ScopedVk<HandleType, ParentHandle, Destroyer> const& handle)
    {
        return !handle.valid();
    }

    /// Inequality comparison between a scoped handle and a null handle
    /// @relates ScopedVk
    template <typename HandleType, typename ParentHandle, typename Destroyer>
    inline bool operator!=(ScopedVk<HandleType, ParentHandle, Destroyer> const& handle, std::nullptr_t const&)
    {
        return handle.valid();
    }

    /// Inequality comparison between a scoped handle and a null handle
    /// @relates ScopedVk
    template <typename HandleType, typename ParentHandle, typename Destroyer>
    inline bool operator!=(std::nullptr_t const&, ScopedVk<HandleType, ParentHandle, Destroyer> const& handle)
    {
        return handle.valid();
    }

    /// Alias to ease use of ScopedVk with handle types whose destroy function is statically available.
    ///
    /// @tparam HandleType The handle type to wrap
    /// @tparam ParentHandle The parent handle type needed by the destroy function.
    /// @tparam DestroyFunction The function used to destroy the handle.
    ///
    /// @see VkDestroyerWithFuncPointer
    ///
    /// @ingroup cts_handle_helpers
    /// @relates ScopedVk
    template <typename HandleType, typename ParentHandle,
              void(VKAPI_PTR* DestroyFunction)(ParentHandle, HandleType, const VkAllocationCallbacks*)>
    using ScopedVkWithDefaultDestroy = ScopedVk<HandleType, ParentHandle, VkDefaultDestroyer<HandleType, ParentHandle, DestroyFunction>>;

    /// Alias to ease use of ScopedVk with handle types whose destroy function is a run-time function pointer (such as from an extension)
    ///
    /// @tparam HandleType The handle type to wrap
    /// @tparam ParentHandle The parent handle type needed by the destroy function.
    ///
    /// @see VkDefaultDestroyer
    ///
    /// @ingroup cts_handle_helpers
    /// @relates ScopedVk
    template <typename HandleType, typename ParentHandle>
    using ScopedVkWithPfn = ScopedVk<HandleType, ParentHandle, VkDestroyerWithFuncPointer<HandleType, ParentHandle>>;

    using ScopedVkDeviceMemory = ScopedVkWithDefaultDestroy<VkDeviceMemory, VkDevice, &vkFreeMemory>;
    using ScopedVkPipeline = ScopedVkWithDefaultDestroy<VkPipeline, VkDevice, &vkDestroyPipeline>;
    using ScopedVkPipelineLayout = ScopedVkWithDefaultDestroy<VkPipelineLayout, VkDevice, &vkDestroyPipelineLayout>;
    using ScopedVkDescriptorSetLayout = ScopedVkWithDefaultDestroy<VkDescriptorSetLayout, VkDevice, &vkDestroyDescriptorSetLayout>;
    using ScopedVkDescriptorPool = ScopedVkWithDefaultDestroy<VkDescriptorPool, VkDevice, &vkDestroyDescriptorPool>;
    using ScopedVkImage = ScopedVkWithDefaultDestroy<VkImage, VkDevice, &vkDestroyImage>;
    using ScopedVkImageView = ScopedVkWithDefaultDestroy<VkImageView, VkDevice, &vkDestroyImageView>;
    using ScopedVkSampler = ScopedVkWithDefaultDestroy<VkSampler, VkDevice, &vkDestroySampler>;
}  // namespace Conformance

#endif
