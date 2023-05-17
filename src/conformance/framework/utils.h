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

#pragma once

#include <stdarg.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <random>
#include <locale>
#include <memory>
#include <mutex>

/// @addtogroup cts_framework
/// @{

/// XRC_DISABLE_GCC_WARNING / XRC_RESTORE_GCC_WARNING
///
/// Portable wrapper for disabling GCC compiler warnings, one at a time.
///
/// Example usage:
///     XRC_DISABLE_GCC_WARNING(-Wmissing-braces)  // Only one warning per usage.
///     XRC_DISABLE_GCC_WARNING(-Wunused-variable)
///     /* code */
///     XRC_RESTORE_GCC_WARNINGS()
///     XRC_RESTORE_GCC_WARNINGS()                 // Must match each disable with a restore.
///
#if !defined(XRC_DISABLE_GCC_WARNING)
#if defined(__GNUC__)
#define XDGW1(x) #x
#define XDGW2(x) XDGW1(GCC diagnostic ignored x)
#define XDGW3(x) XDGW2(#x)
#endif

#if defined(__GNUC__) && (__GNUC_VERSION__ >= 406)
#define XRC_DISABLE_GCC_WARNING(w) _Pragma("GCC diagnostic push") _Pragma(XDGW3(w))
#elif defined(__GNUC__) && (__GNUC_VERSION__ >= 404)  // GCC 4.4 doesn't support diagnostic push, but supports disabling warnings.
#define XRC_DISABLE_GCC_WARNING(w) _Pragma(XDGW3(w))
#else
#define XRC_DISABLE_GCC_WARNING(w)
#endif
#endif  // !defined(XRC_DISABLE_GCC_WARNING)

#if !defined(XRC_RESTORE_GCC_WARNING)
#if defined(__GNUC__) && (__GNUC_VERSION__ >= 4006)
#define XRC_RESTORE_GCC_WARNINGS() _Pragma("GCC diagnostic pop")
#else
#define XRC_RESTORE_GCC_WARNING()
#endif
#endif  // !defined(XRC_RESTORE_GCC_WARNING)

///  XRC_DISABLE_CLANG_WARNING / XRC_RESTORE_CLANG_WARNING
///
/// Portable wrapper for disabling GCC compiler warnings, one at a time.
///
/// Example usage:
///     XRC_DISABLE_CLANG_WARNING(-Wmissing-braces)  // Only one warning per usage.
///     XRC_DISABLE_CLANG_WARNING(-Wunused-variable)
///     /* code */
///     XRC_RESTORE_CLANG_WARNINGS()
///     XRC_RESTORE_CLANG_WARNINGS()                 // Must match each disable with a restore.
///
///
#if !defined(XRC_DISABLE_CLANG_WARNING)
#if defined(__clang__)
#define XDCW1(x) #x
#define XDCW2(x) XDCW1(clang diagnostic ignored x)
#define XDCW3(x) XDCW2(#x)

#define XRC_DISABLE_CLANG_WARNING(w) _Pragma("clang diagnostic push") _Pragma(XDCW3(w))
#else
#define XRC_DISABLE_CLANG_WARNING(w)
#endif
#endif  // !defined(XRC_DISABLE_CLANG_WARNING)

#if !defined(XRC_RESTORE_CLANG_WARNING)
#if defined(__clang__)
#define XRC_RESTORE_CLANG_WARNING() _Pragma("clang diagnostic pop")
#else
#define XRC_RESTORE_CLANG_WARNING()
#endif
#endif  // !defined(XRC_RESTORE_CLANG_WARNING)

/// XRC_DISABLE_MSVC_WARNING / XRC_RESTORE_MSVC_WARNING
///
/// Portable wrapper for disabling VC++ compiler warnings.
///
/// Example usage:
///     XRC_DISABLE_MSVC_WARNING(4556 4782 4422)
///     /* code */
///     XRC_RESTORE_MSVC_WARNING()
///
#if !defined(XRC_DISABLE_MSVC_WARNING)
#if defined(_MSC_VER)
#define XRC_DISABLE_MSVC_WARNING(w) __pragma(warning(push)) __pragma(warning(disable : w))
#else
#define XRC_DISABLE_MSVC_WARNING(w)
#endif
#endif  // !defined(XRC_DISABLE_MSVC_WARNING)

#if !defined(XRC_RESTORE_MSVC_WARNING)
#if defined(_MSC_VER)
#define XRC_RESTORE_MSVC_WARNING() __pragma(warning(pop))
#else
#define XRC_RESTORE_MSVC_WARNING()
#endif
#endif  // !defined(XRC_RESTORE_MSVC_WARNING)

// -----------------------------------------------------------------------------------
// ***** XRC_SUPPRESS_MSVC_WARNING
///
/// Portable wrapper for disabling a single warning on the next source code line.
///
/// Example usage:
///     XRC_SUPPRESS_MSVC_WARNING(4556)
///     /* code */
///
#if !defined(XRC_SUPPRESS_MSVC_WARNING)
#if defined(_MSC_VER)
#define XRC_SUPPRESS_MSVC_WARNING(w) __pragma(warning(suppress : w))
#else
#define XRC_SUPPRESS_MSVC_WARNING(w)
#endif
#endif  // !defined(XRC_SUPPRESS_MSVC_WARNING)

/// XRC_DISABLE_ALL_MSVC_WARNINGS / XRC_RESTORE_ALL_MSVC_WARNINGS
///
/// Portable wrapper for disabling all VC++ compiler warnings.
/// XRC_RESTORE_ALL_MSVC_WARNINGS restores warnings that were disabled by
/// XRC_DISABLE_ALL_MSVC_WARNINGS. Any previously enabled warnings will still be
/// enabled after XRC_RESTORE_ALL_MSVC_WARNINGS.
///
/// Example usage:
///     XRC_DISABLE_ALL_MSVC_WARNINGS()
///     /* code */
///     XRC_RESTORE_ALL_MSVC_WARNINGS()

#if !defined(XRC_DISABLE_ALL_MSVC_WARNINGS)
#if defined(_MSC_VER)
#define XRC_DISABLE_ALL_MSVC_WARNINGS() __pragma(warning(push, 0)) __pragma(warning(disable : 4263 4264 4265 4266))
#else
#define XRC_DISABLE_ALL_MSVC_WARNINGS()
#endif
#endif  // !defined(XRC_DISABLE_ALL_MSVC_WARNINGS)

#if !defined(XRC_RESTORE_ALL_MSVC_WARNINGS)
#if defined(_MSC_VER)
#define XRC_RESTORE_ALL_MSVC_WARNINGS() __pragma(warning(pop))
#else
#define XRC_RESTORE_ALL_MSVC_WARNINGS()
#endif
#endif  // !defined(XRC_RESTORE_ALL_MSVC_WARNINGS)

/// XRC_STRINGIFY
///
/// Converts a preprocessor symbol to a string.
///
/// Example usage:
///     printf("Line: %s", XRC_STRINGIFY(__LINE__));
///
#if !defined(XRC_STRINGIFY)
#define XRC_STRINGIFY_IMPL(x) #x
#define XRC_STRINGIFY(x) XRC_STRINGIFY_IMPL(x)
#endif  // !defined(XRC_STRINGIFY)

/// XRC_ENUM_NAME_PAIR
///
/// Converts an enum name to a enum, const char* tuple.
///
/// Example usage:
///     std::pair<SomeEnum, const char*> pair = XRC_ENUM_NAME_PAIR(e, 5);
///
#if !defined(XRC_ENUM_NAME_PAIR)
#define XRC_ENUM_NAME_PAIR(e, _) {e, XRC_STRINGIFY(e)},
#endif  // !defined(XRC_ENUM_NAME_PAIR)

/// strequal
///
/// Portable C string case-sensitive compare for ANSI-only strings.
///
#define strequal(a, b) (strcmp(a, b) == 0)

/// striequal
///
/// Portable C string case-insensitive compare for ANSI-only strings.
///
#ifdef _MSC_VER
#define striequal(a, b) (_stricmp(a, b) == 0)
#else
#define striequal(a, b) (strcasecmp(a, b) == 0)
#endif

namespace Conformance
{
    // Copied directly from the helloXr project. Let's get to a point in the future where all these
    // functions are moved to a central location with consistent usage.
    struct IgnoreCaseStringLess
    {
        bool operator()(const std::string& a, const std::string& b, const std::locale& loc = std::locale()) const noexcept
        {
            const std::ctype<char>& ctype = std::use_facet<std::ctype<char>>(loc);

            const auto ignoreCaseCharLess = [&](char c1, char c2) { return (ctype.tolower(c1) < ctype.tolower(c2)); };

            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), ignoreCaseCharLess);
        }
    };

    /// Returns a std::string that was initialized via printf-style formatting.
    /// The behavior is undefined if the specified format or arguments are invalid.
    /// Example usage:
    ///     std::string s = StringSprintf("Hello %s", "world");
    std::string StringSprintf(const char* format, ...);

    /// Returns a std::string that was initialized via printf-style formatting.
    /// The behavior is undefined if the specified format or arguments are invalid.
    /// Example usage:
    ///     std::string s = StringVsprintf("Hello %s", args);
    std::string StringVsprintf(const char* format, va_list args);

    /// Returns a std::string that was appended to via printf-style formatting.
    /// The behavior is undefined if the specified format or arguments are invalid.
    /// Example usage:
    ///     AppendSprintf(s, "appended %s", "hello world");
    std::string& AppendSprintf(std::string& s, const char* format, ...);

    /// Changes the case of str, typically for the purpose of exercising case-sensitivity requirements.
    /// Returns a reference to the input str.
    std::string& FlipCase(std::string& str);

    /// SleepMs
    ///
    /// Sleeps the current thread for at least the given milliseconds. Attempt is made to return
    /// immediately after the specified time period, but that cannot be guaranteed and will vary by
    /// some amount in practice.
    ///
    void SleepMs(std::uint32_t ms);

/// This is a specially crafted valid UTF8 string which has four Unicode code points,
/// with the first being one byte, the second being two bytes, the third being three bytes,
/// and the fourth being four bytes. This is useful for exercizing a runtime's requirement
/// of supporting UTF8 strings. See https://tools.ietf.org/html/rfc3629#section-3.
///
#define XRC_UTF8_VALID_EXERCISE_STR "\x61\xC8\xBF\xE5\x86\x98\xF0\xAE\xAA\x85"

/// This is a specially crafted valid UTF8 string which is invalid UTF8. In this case the
/// string is invalid because the C8 byte is followed by an E5 byte, which is unexpected.
///
#define XRC_UTF8_INVALID_EXERCISE_STR "\x61\xC8\xE5\x86\x98"

    /// Implements a thread-safe random number utility, as a thin wrapper around the
    /// C++ rand facility.
    ///
    struct RandEngine
    {
    public:
        RandEngine();
        RandEngine(uint64_t seed);

        /// Sets the new seed, overriding whatever seed was set by the constructor.
        void SetSeed(uint64_t seed);

        /// Returns the seed set by the constructor or the last SetSeed call.
        uint64_t GetSeed() const;

        /// Generates a random size_t within the given range of [begin, end)
        /// Requires that end > begin (i.e. the rand is non-empty).
        /// Does not guarantee perfect uniform distribution.
        ///
        /// Example usage:
        ///     size_t i = RandSizeT(0, container.size());
        ///
        size_t RandSizeT(size_t min, size_t max);

        /// Generates a random int64_t within the given range of [begin, end)
        /// Requires that end > begin (i.e. the rand is non-empty).
        /// Does not guarantee perfect uniform distribution.
        ///
        int64_t RandInt64(int64_t min, int64_t max);
        uint64_t RandUint64(uint64_t min, uint64_t max);

        /// Generates a random int32_t within the given range of [begin, end)
        /// Requires that end > begin (i.e. the rand is non-empty).
        /// Does not guarantee perfect uniform distribution.
        ///
        int32_t RandInt32(int32_t min, int32_t max);
        uint32_t RandUint32(uint32_t min, uint32_t max);

    protected:
        mutable std::mutex randEngineMutex;
        uint64_t seed;  // Needs to be manually saved, since C++ engines don't have a get-seed function.
        std::mt19937_64 engine;
    };

    /// Validates that the string is valid UTF-8 encoded.
    ///
    /// Example usage:
    ///    REQUIRE(ValidateStringUTF8("abcdef", 6));
    ///
    bool ValidateStringUTF8(const char* str, std::size_t length);

    /// Validates that the contents of a char buffer are valid in length and valid UTF-8.
    ///
    /// Example usage:
    ///    char buffer[16] = ...;
    ///    REQUIRE(ValidateFixedSizeString(buffer));
    ///
    template <std::size_t N>
    bool ValidateFixedSizeString(const char (&str)[N], bool mayBeEmpty = true)
    {
        for (size_t i = 0; i < N; ++i) {
            if (str[i] == '\0') {
                if ((i == 0) && !mayBeEmpty) {
                    return false;
                }

                return ValidateStringUTF8(str, i);
            }
        }

        return false;
    }

    /// Given a string of substrings delimited by some delimiter (usually ' '  or ','), convert it
    /// into a vector of the substrings. If append is true then the array is appended to.
    ///
    /// For example:
    ///     "abc def ghi"
    ///         ->
    ///     "abc"
    ///     "def"
    ///     "ghi"
    ///
    void DelimitedStringToStringVector(const char* str, std::vector<std::string>& stringVector, bool append = false, char delimiter = ' ');

    /// Given a vector of strings, convert to a single string with the individual strings separated by
    /// a delimiter character (usually ' '  or ','). If append is true then the output string is appended
    /// to if there are existing entries present.
    ///
    /// For example:
    ///     "abc"
    ///     "def"
    ///     "ghi"
    ///         ->
    ///     "abc def ghi"
    ///
    void StringVectoToDelimitedStringr(const std::vector<std::string>& stringVector, std::string& str, bool append = false,
                                       char delimiter = ' ');

    /// A container for a vector of strings that owns storage for them, and exposes an array of raw pointers.
    ///
    /// All strings supplied are copied.
    struct StringVec
    {
    public:
        StringVec() = default;
        StringVec(StringVec&&) = default;
        StringVec& operator=(StringVec&&) = default;

        /// Copy-construct
        StringVec(StringVec const& other);

        /// Copy-assign
        StringVec& operator=(StringVec const& other);

        /// Construct from vector of std::string.
        StringVec(std::vector<std::string> const& other);

        /// Assign from vector of std::string.
        StringVec& operator=(std::vector<std::string> const& other);

        using inner = std::vector<const char*>;
        using const_iterator = typename inner::const_iterator;

        /// "Conversion" operator to the contained vector of C string pointers.
        operator inner const &() const
        {
            return strPtrVector;
        }

        const char* operator[](size_t i) const
        {
            return strPtrVector[i];
        }
        bool empty() const
        {
            return strPtrVector.empty();
        }
        uint32_t size() const
        {
            return static_cast<uint32_t>(strPtrVector.size());
        }
        const char* const* data() const
        {
            return strPtrVector.data();
        }
        const_iterator begin() const
        {
            return strPtrVector.begin();
        }
        const_iterator end() const
        {
            return strPtrVector.end();
        }

        // Returns true if the string exists in this vector (case-sensitive)
        bool contains(std::string const& str) const;

        void push_back(const char* str);
        void push_back(std::string const& str)
        {
            push_back(str.c_str());
        }

        // Adds the specified string to the container only if it does not already exist (case-sensitive).
        void push_back_unique(std::string const& str);
        void push_back_unique(const char* str);

        void set(size_t i, const char* str);
        void set(size_t i, std::string const& str)
        {
            set(i, str.c_str());
        }

        void erase(const_iterator it);

        void clear();

    private:
        static std::unique_ptr<char[]> copyString(const char* str);
        void rebuild();
        std::vector<std::unique_ptr<char[]>> strOwnVector;
        inner strPtrVector;
    };

    struct Size2D
    {
        uint32_t w;
        uint32_t h;
    };

}  // namespace Conformance

/// @}
