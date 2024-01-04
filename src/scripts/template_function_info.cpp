// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Used in conformance tests.

#include "conformance_utils.h"

namespace Conformance {

// This is a generated list of information about functions.
static const FunctionInfoMap functionInfoMapInternal{
//# for cur_cmd in sorted_cmds
    {/*{ cur_cmd.name | quote_string }*/,
     FunctionInfo(PFN_xrVoidFunction(nullptr),
//# if cur_cmd.name in null_instance_ok
     true, /* null instance OK */
//# else
     false, /* null instance not OK */
//# endif
//# if cur_cmd.ext_name and "XR_VERSION_" not in cur_cmd.ext_name
     /*{ cur_cmd.ext_name | quote_string }*/, /* extension required */
//# else
     nullptr,
//# endif
     {/*{ gen.allReturnCodesForCommand(cur_cmd) | join(', ') }*/})},
//# endfor
};

const FunctionInfoMap& GetFunctionInfoMap()
{
    return functionInfoMapInternal;
}

} // namespace Conformance
