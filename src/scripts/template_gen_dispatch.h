// Copyright (c) 2017-2021, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Used in conformance layer.

#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <functional>
#include <mutex>
#include <vector>
#include <unordered_map>
#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "xr_generated_dispatch_table.h"
#include <loader_interfaces.h>

// HandleState.h contains non-generated code.
#include "HandleState.h"

/*
 * Generated conversion functions from handles to integers.
 *
 * Explicitly generated to avoid a cast silently permitting the wrong thing.
 *
 */

#if XR_PTR_SIZE == 4
/* All handles are just uint64_t typedefs in 32-bit environments */
static inline IntHandle HandleToInt(uint64_t h)
{
    return h;
}

#else
/* Real unique types (pointers) on 64-bit */

//# for handle in gen.api_handles
/*{ protect_begin(handle) }*/
static inline IntHandle HandleToInt(/*{ handle.name }*/ h)
{
    return reinterpret_cast<IntHandle>(h);
}
/*{ protect_end(handle) }*/
//# endfor
#endif

struct EnabledExtensions {
    EnabledExtensions(const XrInstanceCreateInfo* createInfo) {
        auto isEnabled = [&](const char* extName) {
            auto end = createInfo->enabledExtensionNames + createInfo->enabledExtensionCount;
            auto it = std::find_if(createInfo->enabledExtensionNames, end, [&](const char* enabledExt) { return strcmp(enabledExt, extName) == 0; });
            return it != end;
        };

//# for ext in registry.extdict
        /*{ ext | make_ext_variable_name }*/ = isEnabled(/*{ ext | quote_string }*/);
//# endfor
    }

//# for ext in registry.extdict
    bool /*{ ext | make_ext_variable_name }*/{false};
//# endfor
};

struct ConformanceHooksBase {
    ConformanceHooksBase(XrInstance instance, XrGeneratedDispatchTable dispatchTable, EnabledExtensions enabledExtensions)
        : instance(instance), dispatchTable(dispatchTable), enabledExtensions(enabledExtensions) {
        }

    virtual ~ConformanceHooksBase() = default;
    virtual void ConformanceFailure(XrDebugUtilsMessageSeverityFlagsEXT severity, const char* functionName, const char* fmtMessage, ...) = 0;

//# for cur_cmd in sorted_cmds
//#     if cur_cmd.name not in skip_hooks and cur_cmd.name != "xrGetInstanceProcAddr"
/*{ protect_begin(cur_cmd) }*/
    /*{ cur_cmd.cdecl | collapse_whitespace
    | replace("XRAPI_ATTR XrResult XRAPI_CALL xr", "virtual XrResult xr")
    | replace(";", "")
}*/;
/*{ protect_end(cur_cmd) }*/
//#     endif
//# endfor

    const XrInstance instance;
    const XrGeneratedDispatchTable dispatchTable;
    const EnabledExtensions enabledExtensions;
};

XRAPI_ATTR XrResult XRAPI_CALL ConformanceLayer_xrGetInstanceProcAddr(
    XrInstance instance,
    const char* name,
    PFN_xrVoidFunction* function);

