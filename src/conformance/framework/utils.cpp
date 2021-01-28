// Copyright (c) 2019-2021, The Khronos Group Inc.
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

#include "utils.h"
#include "platform_utils.hpp"
#include <string>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <thread>
#include <algorithm>
#include <ctype.h>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Conformance
{

    std::string StringSprintf(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string s = StringVsprintf(format, args);
        va_end(args);
        return s;
    }

    std::string StringVsprintf(const char* format, va_list args)
    {
        char buffer[512];  // We first try writing into this buffer. If it's not enough then use a string.

        va_list tmp_args;
        va_copy(tmp_args, args);
        const int requiredStrlen = std::vsnprintf(buffer, sizeof(buffer), format, tmp_args);
        va_end(tmp_args);

        if (requiredStrlen < (int)sizeof(buffer)) {  // If the entire result fits into the buffer.
            return std::string(buffer, requiredStrlen);
        }

        std::string result(requiredStrlen, '\0');
        std::vsnprintf(&result[0], result.size(), format, args);

        return result;
    }

    std::string& AppendSprintf(std::string& s, const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        s += StringVsprintf(format, args);
        va_end(args);
        return s;
    }

    std::string& FlipCase(std::string& str)
    {
        for (char& i : str) {
            if (islower(i) != 0) {
                i = (char)toupper(i);
            }
            else {
                i = (char)tolower(i);
            }
        }

        return str;
    }

    // Provides a managed set of random number generators. Currently the usage of these generators
    // is imperfect because modulus (%) operations are done against their results, which introduces
    // a slight skew in the distribution for most ranges. C++ random number generation requires
    // reconstructing distribution classes for each range used, which is onerous.
    //
    RandEngine::RandEngine() : randEngineMutex(), seed(std::time(NULL)), engine()  // Default seed
    {
    }

    RandEngine::RandEngine(uint64_t seed_) : randEngineMutex(), seed(seed_), engine(seed_)
    {
    }

    void RandEngine::SetSeed(uint64_t seed_)
    {
        std::unique_lock<std::mutex> lock(randEngineMutex);
        seed = seed_;
        engine.seed(seed_);
    }

    uint64_t RandEngine::GetSeed() const
    {
        std::unique_lock<std::mutex> lock(randEngineMutex);
        return seed;
    }

    size_t RandEngine::RandSizeT(size_t min, size_t max)
    {
        std::unique_lock<std::mutex> lock(randEngineMutex);
        std::uniform_int_distribution<size_t> randDistSizeT(min, max - 1);
        return randDistSizeT(engine);
    }

    int64_t RandEngine::RandInt64(int64_t min, int64_t max)
    {
        std::unique_lock<std::mutex> lock(randEngineMutex);
        std::uniform_int_distribution<int64_t> randDistSizeT(min, max - 1);
        return randDistSizeT(engine);
    }

    uint64_t RandEngine::RandUint64(uint64_t min, uint64_t max)
    {
        std::unique_lock<std::mutex> lock(randEngineMutex);
        std::uniform_int_distribution<uint64_t> randDistSizeT(min, max - 1);
        return randDistSizeT(engine);
    }

    int32_t RandEngine::RandInt32(int32_t min, int32_t max)
    {
        std::unique_lock<std::mutex> lock(randEngineMutex);
        std::uniform_int_distribution<int32_t> randDistSizeT(min, max - 1);
        return randDistSizeT(engine);
    }

    uint32_t RandEngine::RandUint32(uint32_t min, uint32_t max)
    {
        std::unique_lock<std::mutex> lock(randEngineMutex);
        std::uniform_int_distribution<uint32_t> randDistSizeT(min, max - 1);
        return randDistSizeT(engine);
    }

    bool ValidateStringUTF8(const char* str, size_t length)
    {
        for (size_t i = 0; i < length; ++i) {
            size_t end = 0;

            if ((str[i] & 0b10000000) == 0b00000000) {
                end = i + 1;
            }
            else if ((str[i] & 0b11100000) == 0b11000000) {
                end = i + 2;
            }
            else if ((str[i] & 0b11110000) == 0b11100000) {
                end = i + 3;
            }
            else if ((str[i] & 0b11111000) == 0b11110000) {
                end = i + 4;
            }
            else if ((str[i] & 0b11111100) == 0b11111000) {
                end = i + 5;
            }
            else if ((str[i] & 0b11111110) == 0b11111100) {
                end = i + 6;
            }
            else {
                return false;
            }

            if (end > length) {
                return false;
            }

            while ((i + 1) < end) {
                if ((str[++i] & 0b11000000) != 0b10000000) {
                    return false;
                }
            }
        }

        return true;
    }

    void DelimitedStringToStringVector(const char* str, std::vector<std::string>& stringVector, bool append, char delimiter)
    {
        std::string temp;

        if (!append) {
            stringVector.clear();
        }

        for (const char* p = str; *p != 0; ++p) {
            if ((*p == delimiter) && (!temp.empty())) {
                stringVector.push_back(temp);
                temp.clear();
            }
            else if (*p != delimiter) {
                temp += *p;
            }
        }

        if (!temp.empty()) {
            stringVector.push_back(temp);
        }
    }

    void StringVectoToDelimitedStringr(const std::vector<std::string>& stringVector, std::string& str, bool append, char delimiter)
    {
        if (!append) {
            str.clear();
        }

        for (auto& value : stringVector) {
            if (!str.empty()) {
                str += delimiter;
            }
            str += value;
        }
    }

    void SleepMs(uint32_t ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    StringVec::StringVec(StringVec const& other)
    {
        for (auto& str : other) {
            push_back(str);
        }
    }

    StringVec::StringVec(std::vector<std::string> const& other)
    {
        for (auto& str : other) {
            push_back(str);
        }
    }

    StringVec& StringVec::operator=(StringVec const& other)
    {
        if (this == &other) {
            // self-assign
            return *this;
        }
        // copy
        auto newVec = StringVec{other};

        // swap
        std::swap(strOwnVector, newVec.strOwnVector);
        std::swap(strPtrVector, newVec.strPtrVector);
        return *this;
    }

    StringVec& StringVec::operator=(std::vector<std::string> const& other)
    {
        // copy
        auto newVec = StringVec{other};

        // swap
        std::swap(strOwnVector, newVec.strOwnVector);
        std::swap(strPtrVector, newVec.strPtrVector);
        return *this;
    }

    std::unique_ptr<char[]> StringVec::copyString(const char* str)
    {
        auto len = strlen(str) + 1;
        std::unique_ptr<char[]> copy(new char[len]);
        std::memcpy(copy.get(), str, len);
        return copy;
    }

    bool StringVec::contains(std::string const& str) const
    {
        const auto e = end();
        auto it = std::find(begin(), e, str);
        return it != e;
    }

    void StringVec::push_back(const char* str)
    {
        // Copy the string
        auto copy = copyString(str);
        auto raw = copy.get();
        // Store the unique_ptr that owns the copy, as well as the raw pointer.
        strOwnVector.emplace_back(std::move(copy));
        strPtrVector.push_back(raw);
    }

    void StringVec::push_back_unique(std::string const& str)
    {
        if (!contains(str)) {
            push_back(str);
        }
    }

    void StringVec::push_back_unique(const char* str)
    {
        if (!contains(str)) {
            push_back(str);
        }
    }

    void StringVec::set(size_t i, const char* str)
    {
        if (i > strPtrVector.size()) {
            throw std::out_of_range("out of range when setting string");
        }
        auto copy = copyString(str);
        auto raw = copy.get();
        strOwnVector[i] = std::move(copy);
        strPtrVector[i] = raw;
    }

    void StringVec::clear()
    {
        strPtrVector.clear();
        strOwnVector.clear();
    }

    void StringVec::erase(const_iterator it)
    {
        auto i = std::distance(begin(), it);
        {
            auto it2 = strPtrVector.begin() + i;
            strPtrVector.erase(it2);
        }
        {
            auto it2 = strOwnVector.begin() + i;
            strOwnVector.erase(it2);
        }
    }

    void StringVec::rebuild()
    {
        strPtrVector.clear();
        for (const auto& ptr : strOwnVector) {
            strPtrVector.push_back(ptr.get());
        }
    }

}  // namespace Conformance
