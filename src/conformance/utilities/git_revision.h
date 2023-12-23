// Copyright (c) 2017-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace Conformance
{
    /// A string usable to describe the git revision.
    /// Should be `git describe --dirty "*cts-*"` output for a proper submission.
    extern const char* kGitRevisionString;

    /// Whether the build process successfully queried git for version info relative to a release tag.
    /// Should be true for a proper submission.
    extern const bool kGitRevisionSucceeded;

    /// Whether the revision is exactly a CTS release tag (based on the name).
    extern const bool kGitRevisionExactTag;

    /// Whether the working tree had local changes ("dirty").
    /// Should be false for a proper submission
    extern const bool kGitRevisionLocalChanges;

}  // namespace Conformance
