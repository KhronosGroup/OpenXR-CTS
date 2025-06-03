// Copyright (c) 2024-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include "autoskip.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace Conformance
{

    AutoSkipTimeoutHandler::AutoSkipTimeoutHandler(std::chrono::milliseconds timeout) : m_timeout(timeout)
    {
    }

    bool AutoSkipTimeoutHandler::ProcessInsideLoopAndWarn()
    {
        if (m_timeout == std::chrono::milliseconds(0)) {
            // not doing a timeout
            return false;
        }
        if (!m_hasFirstCall) {
            // Restart the stopwatch on the first call, so we're only counting how long actual frames took
            m_stopwatch.Restart();
            m_hasFirstCall = true;
        }
        bool doAutoSkip = m_stopwatch.Elapsed() > m_timeout;
        if (doAutoSkip) {
            WARN("User-specified timeout reached, skipping/continuing automatically.");
        }
        return doAutoSkip;
    }

}  // namespace Conformance
