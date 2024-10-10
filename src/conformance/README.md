# OpenXR™ Conformance Test Suite (OpenXR-CTS)

<!--
Copyright (c) 2019-2024, The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

The OpenXR Conformance Test Suite is a collection of tests covering the breadth
of the OpenXR API. A runtime passing this test suite is a **necessary** part of
the [OpenXR Adopter Process][adopter], though it is **not sufficient** on its
own to deem a runtime conformant.

It is maintained as open source as a part of the work of the OpenXR Working
Group. It is maintained in two locations:

- The private, internal Khronos GitLab monorepo for OpenXR:
  <https://gitlab.khronos.org/openxr/openxr/>
- A public repository on GitHub containing only the CTS and its dependencies:
  <https://github.com/KhronosGroup/OpenXR-CTS>

Changes from GitHub are periodically imported into the private GitLab. Upon
specification patch release, the public `devel` branch is updated from the
internal repository. Upon approval of a new tagged release of OpenXR-CTS for
conformance submissions, the public `devel` and `approved` branches are updated
from the internal repo and signed tags are published.

[adopter]: https://www.khronos.org/conformance/adopters/

## Usage

For complete instructions on usage, design, and runtime requirements, see the
CTS Usage Guide in the `usage/` (`src/conformance/usage/`) directory.
AsciiDoctor can be used to generate a formatted HTML or PDF file from the
separate chapters:

```sh
# from the usage/ directory

# for an HTML document in generated/out/
make html

# for a PDF document in generated/out/
make pdf
```

HTML and PDF versions of the Usage Guide are also available on GitHub alongside
tagged, approved releases, or on the [OpenXR Registry][registry].

[registry]: https://registry.khronos.org/OpenXR/
