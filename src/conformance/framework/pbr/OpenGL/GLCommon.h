// Copyright 2023-2024, The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once

#include "common/gfxwrapper_opengl.h"

#if defined(APIENTRY) && !defined(GL_APIENTRY)
#define GL_APIENTRY APIENTRY
#endif

#include <type_traits>
#include <utility>

namespace Pbr
{
    static constexpr GLuint GLnull = 0;

    /// A stateless destroyer for OpenGL handles that have a destroy function we can refer to statically.
    ///
    /// @tparam DestroyFunction The function used to destroy the handle.
    ///
    /// @see ScopedGLWithDefaultDestroy
    /// @see ScopedGL
    /// @see GLDestroyerWithFuncPointer
    ///
    /// @ingroup cts_handle_helpers
    template <void(GL_APIENTRY* DestroyFunction)(GLuint)>
    class GLDefaultDestroyer
    {
    public:
        void operator()(GLuint handle) const noexcept
        {
            DestroyFunction(handle);
        }
    };

    /// A destroyer for OpenGL handles that holds state at runtime to contain a function pointer.
    ///
    /// This is mainly for things from extensions.
    ///
    /// @see ScopedGLWithPfn
    /// @see GLDefaultDestroyer
    /// @see ScopedGL
    ///
    /// @ingroup cts_handle_helpers
    class GLDestroyerWithFuncPointer
    {
    public:
        using DestroyFunction = void(GL_APIENTRY*)(GLuint);

        GLDestroyerWithFuncPointer(DestroyFunction pfn) : pfn_(pfn)
        {
        }

        void operator()(GLuint handle) const noexcept
        {
            pfn_(handle);
        }

    private:
        DestroyFunction pfn_;
    };

    /// A unique-ownership RAII helper for OpenGL handles.
    ///
    /// @tparam TagType A tag type to have a little bit of type safety
    /// @tparam Destroyer a functor type that destroys the handle - may be stateless or have state
    ///
    /// @see GLDefaultDestroyer
    /// @see GLDestroyerWithFuncPointer
    ///
    /// @ingroup cts_handle_helpers
    template <typename TagType, typename Destroyer>
    class ScopedGL
    {
    public:
        /// Default (empty) constructor
        ScopedGL() = default;

        /// Empty constructor when we need a destroyer instance.
        explicit ScopedGL(Destroyer d) : d_(d)
        {
        }

        /// Explicit constructor from handle, if we don't need a destroyer instance.
        explicit ScopedGL(GLuint h, std::enable_if<std::is_default_constructible<Destroyer>::value>* = nullptr) : h_(h)
        {
        }

        /// Constructor from handle when we need a destroyer instance.
        ScopedGL(GLuint h, Destroyer d) : h_(h), d_(d)
        {
        }

        /// Destructor
        ~ScopedGL()
        {
            reset();
        }

        /// Non-copyable
        ScopedGL(ScopedGL const&) = delete;

        /// Non-copy-assignable
        ScopedGL& operator=(ScopedGL const&) = delete;

        /// Move-constructible
        ScopedGL(ScopedGL&& other) noexcept : h_(std::move(other.h_)), d_(std::move(other.d_))
        {
            other.clear();
        }

        /// Move-assignable
        ScopedGL& operator=(ScopedGL&& other) noexcept
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
            return get() != GLnull;
        }

        /// Is this handle valid?
        explicit operator bool() const noexcept
        {
            return valid();
        }

        void swap(ScopedGL& other) noexcept
        {
            std::swap(h_, other.h_);
            std::swap(d_, other.d_);
        }

        /// Destroy the owned handle, if any.
        void reset()
        {
            if (get() != GLnull) {
                get_destroyer()(get());
                clear();
            }
        }

        /// Assign a new handle into this object's control, destroying the old one if applicable.
        void adopt(GLuint h)
        {
            reset();
            h_ = h;
        }

        /// Assign a new handle into this object's control, including new destroyer, destroying the old one if applicable.
        void adopt(GLuint h, Destroyer&& d)
        {
            adopt(h);
            d_ = std::move(d);
        }

        /// Access the raw handle without affecting ownership or lifetime.
        GLuint get() const noexcept
        {
            return h_;
        }

        /// Access the destroyer functor
        const Destroyer& get_destroyer() const noexcept
        {
            return d_;
        }

        /// Release the handle from this object's control.
        GLuint release() noexcept
        {
            GLuint ret = h_;
            clear();
            return ret;
        }

        /// Reset and return the address of the handle to be used as an outparam.
        ///
        /// Permissible per 2.3.1 in the OpenGL spec: "If the generating command modifies values through a pointer argument, no change is made to these values."
        GLuint* resetAndPut()
        {
            reset();
            return &h_;
        }

    private:
        void clear() noexcept
        {
            h_ = GLnull;
        }
        GLuint h_ = GLnull;
        Destroyer d_;
    };

    /// Swap function for scoped handles, found using ADL.
    /// @relates ScopedGL
    template <typename TagType, typename Destroyer>
    inline void swap(ScopedGL<TagType, Destroyer>& a, ScopedGL<TagType, Destroyer>& b)
    {
        return a.swap(b);
    }

    /// Equality comparison between a scoped handle and a null handle
    /// @relates ScopedGL
    template <typename TagType, typename Destroyer>
    inline bool operator==(ScopedGL<TagType, Destroyer> const& handle, std::nullptr_t const&)
    {
        return !handle.valid();
    }

    /// Equality comparison between a scoped handle and a null handle
    /// @relates ScopedGL
    template <typename TagType, typename Destroyer>
    inline bool operator==(std::nullptr_t const&, ScopedGL<TagType, Destroyer> const& handle)
    {
        return !handle.valid();
    }

    /// Inequality comparison between a scoped handle and a null handle
    /// @relates ScopedGL
    template <typename TagType, typename Destroyer>
    inline bool operator!=(ScopedGL<TagType, Destroyer> const& handle, std::nullptr_t const&)
    {
        return handle.valid();
    }

    /// Inequality comparison between a scoped handle and a null handle
    /// @relates ScopedGL
    template <typename TagType, typename Destroyer>
    inline bool operator!=(std::nullptr_t const&, ScopedGL<TagType, Destroyer> const& handle)
    {
        return handle.valid();
    }

    /// Alias to ease use of ScopedGL with handle types whose destroy function is statically available.
    ///
    /// @tparam TagType A tag type to have a little bit of type safety
    /// @tparam DestroyFunction The function used to destroy the handle.
    ///
    /// @see GLDestroyerWithFuncPointer
    ///
    /// @ingroup cts_handle_helpers
    /// @relates ScopedGL
    template <typename TagType, void(GL_APIENTRY* DestroyFunction)(GLuint)>
    using ScopedGLWithDefaultDestroy = ScopedGL<TagType, GLDefaultDestroyer<DestroyFunction>>;

    /// Alias to ease use of ScopedGL with handle types whose destroy function is a run-time function pointer (such as from an extension)
    ///
    /// @tparam TagType A tag type to have a little bit of type safety
    ///
    /// @see GLDefaultDestroyer
    ///
    /// @ingroup cts_handle_helpers
    /// @relates ScopedGL
    template <typename TagType>
    using ScopedGLWithPfn = ScopedGL<TagType, GLDestroyerWithFuncPointer>;

    /// Function template to wrap a statically-known deleter function that takes a count (1) and a pointer/array of names.
    ///
    /// @tparam F The address of the actual delete function
    ///
    /// @see ScopedGLWithDefaultDestroy
    ///
    /// @ingroup cts_handle_helpers
    /// @relates ScopedGL
    template <void(GL_APIENTRY* F)(GLsizei, const GLuint*)>
    void GL_APIENTRY destroyOne(GLuint handle)
    {
        F(1, &handle);
    }

    /// Functor wrapping a delete function that wants just the name to delete as a parameter.
    ///
    /// You don't have to know the function pointer at compile time (OK for dynamically loaded OpenGL),
    /// you just need to say where you will put the function pointer when you look it up upon load.
    ///
    /// @tparam PFN Function pointer type
    /// @tparam FunctionExtern statically known address of function pointer for the deleter.
    ///
    /// @see GLDeleterOne if the function actually wants a count and an array/pointer
    /// @see ScopedGL
    ///
    /// @ingroup cts_handle_helpers
    template <typename PFN, const PFN FunctionExtern>
    class GLDeleter
    {
    public:
        GLDeleter() = default;

        void operator()(GLuint handle) const
        {
            (*FunctionExtern)(handle);
        }
    };

    /// Functor wrapping a delete function that wants a "1" as the first parameter and the address of the name as the second.
    ///
    /// These functions typically are designed to support deleting arrays of names.
    ///
    /// You don't have to know the function pointer at compile time (OK for dynamically loaded OpenGL),
    /// you just need to say where you will put the function pointer when you look it up upon load.
    ///
    /// @tparam PFN Function pointer type
    /// @tparam FunctionExtern statically known address of function pointer for the deleter.
    ///
    /// @see GLDeleter if the only parameter is the name to delete.
    /// @see ScopedGL
    ///
    /// @ingroup cts_handle_helpers
    template <typename PFN, const PFN FunctionExtern>
    class GLDeleterOne
    {
    public:
        GLDeleterOne() = default;

        void operator()(GLuint handle) const
        {
            (*FunctionExtern)(1, &handle);
        }
    };

    // TODO remove awkward hack: we load OpenGL at runtime into function pointers,
    // but link directly against a library with symbols for OpenGL ES
#if defined(XR_USE_GRAPHICS_API_OPENGL)
#define XRC_GL_PFN(p) p*
#elif defined(XR_USE_GRAPHICS_API_OPENGL_ES)
#define XRC_GL_PFN(p) p
#endif

    /// GLuint wrapper for use with an OpenGL Shader Program: somewhat type-safe, RAII deletes by calling glDeleteProgram
    /// @ingroup cts_handle_helpers
#if defined(OS_APPLE_MACOS)
    using ScopedGLProgram = ScopedGL<struct GlProgramTag, GLDeleter<decltype(glDeleteProgram), &glDeleteProgram>>;
#else
    using ScopedGLProgram = ScopedGL<struct GlProgramTag, GLDeleter<XRC_GL_PFN(PFNGLDELETEPROGRAMPROC), &glDeleteProgram>>;
#endif

    /// GLuint wrapper for use with an OpenGL Shader: somewhat type-safe, RAII deletes by calling glDeleteShader
    /// @ingroup cts_handle_helpers
#if defined(OS_APPLE_MACOS)
    using ScopedGLShader = ScopedGL<struct GlShaderTag, GLDeleter<decltype(glDeleteShader), &glDeleteShader>>;
#else
    using ScopedGLShader = ScopedGL<struct GlShaderTag, GLDeleter<XRC_GL_PFN(PFNGLDELETESHADERPROC), &glDeleteShader>>;
#endif

    /// GLuint wrapper for use with an OpenGL Texture: somewhat type-safe, RAII deletes by calling glDeleteTextures
    /// @ingroup cts_handle_helpers
    using ScopedGLTexture = ScopedGLWithDefaultDestroy<struct GlTextureTag, &destroyOne<&glDeleteTextures>>;

    /// GLuint wrapper for use with an OpenGL Sampler: somewhat type-safe, RAII deletes by calling glDeleteSamplers
    /// @ingroup cts_handle_helpers
#if defined(OS_APPLE_MACOS)
    using ScopedGLSampler = ScopedGL<struct GlSamplerTag, GLDeleterOne<decltype(glDeleteSamplers), &glDeleteSamplers>>;
#else
    using ScopedGLSampler = ScopedGL<struct GlSamplerTag, GLDeleterOne<XRC_GL_PFN(PFNGLDELETESAMPLERSPROC), &glDeleteSamplers>>;
#endif
    /// GLuint wrapper for use with an OpenGL Buffer: somewhat type-safe, RAII deletes by calling glDeleteBuffers
    /// @ingroup cts_handle_helpers
#if defined(OS_APPLE_MACOS)
    using ScopedGLBuffer = ScopedGL<struct GlBufferTag, GLDeleterOne<decltype(glDeleteBuffers), &glDeleteBuffers>>;
#else
    using ScopedGLBuffer = ScopedGL<struct GlBufferTag, GLDeleterOne<XRC_GL_PFN(PFNGLDELETEBUFFERSPROC), &glDeleteBuffers>>;
#endif

    /// GLuint wrapper for use with an OpenGL Vertex Array: somewhat type-safe, RAII deletes by calling glDeleteVertexArrays
    /// @ingroup cts_handle_helpers
#if defined(OS_APPLE_MACOS)
    using ScopedGLVertexArray = ScopedGL<struct GlVertexArrayTag, GLDeleterOne<decltype(glDeleteVertexArrays), &glDeleteVertexArrays>>;
#else
    using ScopedGLVertexArray =
        ScopedGL<struct GlVertexArrayTag, GLDeleterOne<XRC_GL_PFN(PFNGLDELETEVERTEXARRAYSPROC), &glDeleteVertexArrays>>;
#endif

}  // namespace Pbr
