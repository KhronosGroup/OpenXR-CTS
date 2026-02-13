// Copyright 2023-2026 The Khronos Group Inc.
//
// Based in part on code that is:
//
// Copyright (C) Microsoft Corporation.  All Rights Reserved
// Licensed under the MIT License. See License.txt in the project root for license information.
//
// SPDX-License-Identifier: MIT AND Apache-2.0
#pragma once

#include <glad/gl.h>

#include <type_traits>
#include <utility>

namespace Pbr
{
    static constexpr GLuint GLnull = 0;

    /// A unique-ownership RAII helper for OpenGL handles.
    ///
    /// @tparam TagType A tag type to have a little bit of type safety, as a treat
    /// @tparam Destroyer a stateless functor type that destroys the handle
    ///
    /// @ingroup cts_handle_helpers
    template <typename TagType, typename Destroyer>
    class ScopedGL
    {
        static_assert(std::is_default_constructible<Destroyer>::value, "Destroyer must be default constructible");

    public:
        /// Default (empty) constructor
        ScopedGL() noexcept = default;

        /// Explicit constructor from handle
        explicit ScopedGL(GLuint h) noexcept : h_(h)
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
        ScopedGL(ScopedGL&& other) noexcept : ScopedGL()
        {
            swap(other);
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

        /// Access the raw handle without affecting ownership or lifetime.
        GLuint get() const noexcept
        {
            return h_;
        }

        /// Access the destroyer functor
        Destroyer get_destroyer() const noexcept
        {
            return Destroyer{};
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

    /// Function pointer type for any GL function that just deletes one handle (name)
    using PFN_glDeleteName = void(GLAD_API_PTR*)(GLuint);

    /// Functor wrapping a delete function that wants just the name to delete as a parameter.
    ///
    /// You don't have to know the function pointer at compile time (OK for dynamically loaded OpenGL),
    /// you just need to say where you will put the function pointer when you look it up upon load.
    ///
    /// @tparam FunctionExtern statically known address of function pointer for the deleter.
    ///
    /// @see GLDeleterOne if the function actually wants a count and an array/pointer
    /// @see ScopedGL
    ///
    /// @ingroup cts_handle_helpers
    template <const PFN_glDeleteName* FunctionExtern>
    class GLDeleter
    {
    public:
        GLDeleter() noexcept = default;

        void operator()(GLuint handle) const noexcept
        {
            (*FunctionExtern)(handle);
        }
    };

    /// Function pointer type for any GL function that deletes an array of handles (names) taking array size first
    using PFN_glDeleteNameArray = void(GLAD_API_PTR*)(GLsizei n, const GLuint*);

    /// Functor wrapping a delete function that wants a "1" as the first parameter and the address of the name as the second.
    ///
    /// These functions typically are designed to support deleting arrays of names.
    ///
    /// You don't have to know the function pointer at compile time (OK for dynamically loaded OpenGL),
    /// you just need to say where you will put the function pointer when you look it up upon load.
    ///
    /// @tparam FunctionExtern statically known address of function pointer for the deleter.
    ///
    /// @see GLDeleter if the only parameter is the name to delete.
    /// @see ScopedGL
    ///
    /// @ingroup cts_handle_helpers
    template <PFN_glDeleteNameArray* FunctionExtern>
    class GLDeleterOne
    {
    public:
        GLDeleterOne() noexcept = default;

        void operator()(GLuint handle) const noexcept
        {
            (*FunctionExtern)(1, &handle);
        }
    };

    /// GLuint wrapper for use with an OpenGL Shader Program: somewhat type-safe, RAII deletes by calling glDeleteProgram
    /// @ingroup cts_handle_helpers
    using ScopedGLProgram = ScopedGL<struct GlProgramTag, GLDeleter<&glDeleteProgram>>;

    /// GLuint wrapper for use with an OpenGL Shader: somewhat type-safe, RAII deletes by calling glDeleteShader
    /// @ingroup cts_handle_helpers
    using ScopedGLShader = ScopedGL<struct GlShaderTag, GLDeleter<&glDeleteShader>>;

    /// GLuint wrapper for use with an OpenGL Texture: somewhat type-safe, RAII deletes by calling glDeleteTextures
    /// @ingroup cts_handle_helpers
    using ScopedGLTexture = ScopedGL<struct GlTextureTag, GLDeleterOne<&glDeleteTextures>>;

    /// GLuint wrapper for use with an OpenGL Sampler: somewhat type-safe, RAII deletes by calling glDeleteSamplers
    /// @ingroup cts_handle_helpers
    using ScopedGLSampler = ScopedGL<struct GlSamplerTag, GLDeleterOne<&glDeleteSamplers>>;

    /// GLuint wrapper for use with an OpenGL Buffer: somewhat type-safe, RAII deletes by calling glDeleteBuffers
    /// @ingroup cts_handle_helpers
    using ScopedGLBuffer = ScopedGL<struct GlBufferTag, GLDeleterOne<&glDeleteBuffers>>;

    /// GLuint wrapper for use with an OpenGL Vertex Array: somewhat type-safe, RAII deletes by calling glDeleteVertexArrays
    /// @ingroup cts_handle_helpers
    using ScopedGLVertexArray = ScopedGL<struct GlVertexArrayTag, GLDeleterOne<&glDeleteVertexArrays>>;

}  // namespace Pbr
