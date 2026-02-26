// Copyright (c) 2017-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Used in conformance tests.

#include "conformance_utils.h"

namespace Conformance {

// This is a generated list of information about functions.
static const FunctionInfoMap functionInfoMapInternal{
//# for cur_cmd in sorted_cmds
//# if cur_cmd.ext_name != "XR_LOADER_VERSION_1_0"
    {/*{ cur_cmd.name | quote_string }*/,
     FunctionInfo(
//# if cur_cmd.name in null_instance_ok
     true, /* null instance OK */
//# else
     false, /* null instance not OK */
//# endif
//# if cur_cmd.ext_name and "XR_VERSION_1_0" in cur_cmd.ext_name
     XR_MAKE_VERSION(1, 0, 0),
//# elif cur_cmd.ext_name and "XR_VERSION_1_1" in cur_cmd.ext_name
     XR_MAKE_VERSION(1, 1, 0),
//# else
     XrVersion{},
//# endif

//# if cur_cmd.ext_name and "XR_VERSION_" not in cur_cmd.ext_name
     /*{ cur_cmd.ext_name | quote_string }*/, /* extension required */
//# else
     nullptr,
//# endif
     {/*{ gen.allReturnCodesForCommand(cur_cmd) | join(', ') }*/})},
//# endif
//# endfor
};

const FunctionInfoMap& GetFunctionInfoMap()
{
    return functionInfoMapInternal;
}

} // namespace Conformance
