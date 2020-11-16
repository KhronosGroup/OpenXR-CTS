// Copyright (c) 2017-2020 The Khronos Group Inc
//
// SPDX-License-Identifier: Apache-2.0

#include "gen_dispatch.h"

// Unhandled exception at ABI is a catastrophic error in the layer (a bug).
#define ABI_CATCH \
    catch (...) { \
        abort(); /* Something went wrong in the layer. */ \
    }

/*% macro checkExtCode(ext_code) %*/(handleState->enabledExtensions->/*{make_ext_variable_name(ext_code.extension)}*/ && result == /*{ ext_code.value }*/)/*% endmacro %*/
/*% macro checkResult(val) %*/(result == /*{val}*/)/*% endmacro %*/

//# set ext_return_codes = registry.commandextensionsuccesses + registry.commandextensionerrors

//# for cur_cmd in sorted_cmds
//#     if cur_cmd.name not in skip_hooks and cur_cmd.name != "xrGetInstanceProcAddr"

//#         set handle_param = cur_cmd.params[0]
//#         set first_handle_name = gen.getFirstHandleName(handle_param)
//#         set handle_type = handle_param.type
/*{ protect_begin(cur_cmd) }*/

/*{ cur_cmd.cdecl | collapse_whitespace | replace(" xr", " XRAPI_CALL ConformanceLayer_xr") | replace(";", "")
}*/ {
//#         set first_param_object_type = gen.genXrObjectType(handle_type)
    try {
        HandleState* const handleState = GetHandleState({HandleToInt(/*{first_handle_name}*/), /*{first_param_object_type}*/});

        return handleState->conformanceHooks->/*{cur_cmd.name}*/(/*{ cur_cmd.params | map(attribute="name") | join(", ") }*/);
    }
    ABI_CATCH
}

//##
//## Generate the ConformanceHooksBase virtual method
//##
/*{ cur_cmd.cdecl | collapse_whitespace | replace("XrResult xr", "XrResult ConformanceHooksBase::xr") | replace(";", "")
}*/ {
    const /*{cur_cmd.return_type.text}*/ result =  this->dispatchTable./*{ cur_cmd.name | base_name }*/(/*{ cur_cmd.params | map(attribute="name") | join(", ") }*/);

//## TODO: Inspect out structs
//## Check if the return code is a valid return code.
//## Leading false allows each generated entry to start with ||
    bool recognizedReturnCode = (false
//## Core return codes
                /*% for val in cur_cmd.return_values %*/ || /*{ checkResult(val) }*/ /*% endfor %*/

//## Extension return codes, if any
//#             for ext_code in ext_return_codes
//#                 if ext_code.command == cur_cmd.name
                || /*{ checkExtCode(ext_code) }*/ 
//#                 endif
//#             endfor
                );
    if (!recognizedReturnCode) {
        this->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, /*{ cur_cmd.name | quote_string }*/, "Illegal result code returned: %d", result);
    }

//## If this is a create command, we have to create an entry in the appropriate
//## unordered_map pointing to the correct dispatch table for the newly created
//## object.  Likewise, if it's a delete command, we have to remove the entry
//## for the dispatch table from the unordered_map
//#         set is_last_arg_handle = (cur_cmd.params[-1].is_handle)
//#         set is_create = (("xrCreate" in cur_cmd.name) and is_last_arg_handle)
//#         set is_destroy = (("xrDestroy" in cur_cmd.name) and is_last_arg_handle)
//#         if is_create or is_destroy
    if (XR_SUCCEEDED(result)) {
//#             set last_param_name = cur_cmd.params[-1].name
//#             set last_param_type = cur_cmd.params[-1].type
//#             set last_param_object_type = gen.genXrObjectType(last_param_type)
//#             if is_create
        HandleState* const parentHandleState = GetHandleState(HandleStateKey{HandleToInt(/*{first_handle_name}*/), /*{first_param_object_type}*/});
        RegisterHandleState(parentHandleState->CloneForChild(HandleToInt(* /*{last_param_name}*/), /*{last_param_object_type}*/));
//#             endif
//#             if is_destroy
        UnregisterHandleState({HandleToInt(/*{last_param_name}*/), /*{last_param_object_type}*/});
//#             endif
    }
//#         endif

    return result;
}

/*{ protect_end(cur_cmd) }*/

//#     endif
//# endfor
XRAPI_ATTR XrResult XRAPI_CALL ConformanceLayer_xrGetInstanceProcAddr(
    XrInstance                                  instance,
    const char*                                 name,
    PFN_xrVoidFunction*                         function) try {

    HandleState* const handleState = GetHandleState({ HandleToInt(instance), XR_OBJECT_TYPE_INSTANCE });

    if (strcmp(name, "xrGetInstanceProcAddr") == 0) {
        *function = reinterpret_cast<PFN_xrVoidFunction>(ConformanceLayer_xrGetInstanceProcAddr);

//# for cur_cmd in sorted_cmds
//#     set is_core = "XR_VERSION_" in cur_cmd.ext_name
//#     if cur_cmd.name not in skip_hooks and cur_cmd.name != "xrGetInstanceProcAddr"

/*{ protect_begin(cur_cmd) }*/
    } else if (strcmp(name, /*{cur_cmd.name | quote_string}*/) == 0) {
//#         if not is_core
        if (handleState->conformanceHooks->enabledExtensions./*{cur_cmd.ext_name | make_ext_variable_name}*/) {
//#         endif
            *function = reinterpret_cast<PFN_xrVoidFunction>(ConformanceLayer_/*{cur_cmd.name}*/);
//#         if not is_core
        }
//#         endif
/*{ protect_end(cur_cmd) }*/
//#     endif
//# endfor
    }
    if (*function != nullptr) {
        return XR_SUCCESS;
    }

    // We have not found it, so pass it down to the next layer/runtime
    return handleState->conformanceHooks->dispatchTable.GetInstanceProcAddr(instance, name, function);
}
ABI_CATCH
