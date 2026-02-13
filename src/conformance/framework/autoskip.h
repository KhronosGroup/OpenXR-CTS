// Copyright (c) 2024-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "conformance_utils.h"

#include <chrono>

namespace Conformance
{

    /// Encapsulates the handling of auto-skip logic.
    class AutoSkipTimeoutHandler
    {
    public:
        /// Create auto-skip timeout handler from a millisecond timeout.
        ///
        /// Passing 0 means do not perform a timeout.
        explicit AutoSkipTimeoutHandler(std::chrono::milliseconds timeout);

        /// If you want to handle the skip part yourself, call this inside your frame loop.
        ///
        /// This records a WARN() message if you should skip, but does not do the actual skip for you.
        /// The warn makes the result clearly not valid for conformance submission, but is not an error itself.
        /// If it did not call WARN, only SKIP, it might look acceptable to submit, which it is not.
        ///
        /// @returns true if you should skip
        bool ProcessInsideLoopAndWarn();

    private:
        std::chrono::milliseconds m_timeout;
        bool m_hasFirstCall{false};
        Stopwatch m_stopwatch;
    };

}  // namespace Conformance
