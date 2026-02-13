// Copyright (c) 2017-2026 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Used in conformance layer.

#include "gen_dispatch.h"

#if defined(ANDROID)
#define LOG_TAG "XrApiLayer_runtime_conformance"
#include "android_logging.h"
#define LOG_ERROR(...) ALOGE(__VA_ARGS__)
#define LOG_FATAL(...) ALOGF(__VA_ARGS__)
#else
#include <cstdio>
#define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define LOG_FATAL(...) LOG_ERROR(__VA_ARGS__)
#endif

#include <exception>

// Unhandled exception at ABI is a catastrophic error in the layer (a bug).
#define ABI_CATCH                                                                                               \
    catch (const HandleNotFoundException& e) {                                                                  \
        LOG_ERROR("ERROR: Conformance Layer: Unknown handle used, created by unrecognized API call? Message = %s\n", e.what()); \
        return XR_ERROR_HANDLE_INVALID;                                                                         \
    }                                                                                                           \
    catch (const std::exception& e) {                                                                           \
        LOG_FATAL("FATAL: Conformance Layer Bug: caught exception at ABI level with message = %s\n", e.what()); \
        abort(); /* Something went wrong in the layer. */                                                       \
    }                                                                                                           \
    catch (...) {                                                                                               \
        LOG_FATAL("FATAL: Conformance Layer Bug: caught exception at ABI level\n");                             \
        abort(); /* Something went wrong in the layer. */                                                       \
    }

//## Generates an expression that is true if result is the given code, and the corresponding extension is enabled.
/*% macro checkExtCode(ext_code) -%*/
(handleState->conformanceHooks->enabledExtensions.
    /*{- ext_code.extension | make_ext_variable_name }*/ && result == /*{ ext_code.value }*/)
/*%- endmacro %*/
//## Generates an expression that is true if result is the given code
/*%- macro checkResult(val) %*/(result == /*{val}*/)/*% endmacro %*/


//# for cur_cmd in sorted_cmds
//#     if cur_cmd.name not in skip_hooks and cur_cmd.name != "xrGetInstanceProcAddr"

//#         set handle_param = cur_cmd.params[0]
//#         set first_handle_name = gen.getFirstHandleName(handle_param)
//#         set handle_type = handle_param.type
/*{ protect_begin(cur_cmd) }*/

static /*{ cur_cmd.cdecl | collapse_whitespace | replace(" xr", " ConformanceLayer_xr") | replace(";", "")
}*/ {
//#         set first_handle_object_type = gen.genXrObjectType(handle_type)
    try {
        HandleState* const handleState = GetHandleState({HandleToInt(/*{ first_handle_name }*/), /*{ first_handle_object_type }*/});

        return handleState->conformanceHooks->/*{cur_cmd.name}*/(handleState, /*{ cur_cmd.params | map(attribute="name") | join(", ") }*/);
    }
    ABI_CATCH
}

//##
//## Generate the ConformanceHooksBase virtual method
//##
/*{ cur_cmd.cdecl
    | collapse_whitespace
    | replace("XRAPI_ATTR XrResult XRAPI_CALL xr", "XrResult ConformanceHooksBase::xr")
    | replace("(", "(HandleState* const handleState, ")
    | replace(";", "")
}*/
{
    //## Possibly unused
    (void)handleState;
    //## Ensure that the function is implemented by the runtime or its a validation error instead of a segfault caused by a nullptr dereference
    if (this->dispatchTable./*{ cur_cmd.name | base_name }*/ == nullptr) {
        this->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, "/*{ cur_cmd.name | base_name }*/", "Function is not implemented in runtime");
        return XR_ERROR_VALIDATION_FAILURE;
    }

    const /*{cur_cmd.return_type.text}*/ result =  this->dispatchTable./*{ cur_cmd.name | base_name }*/(/*{ cur_cmd.params | map(attribute="name") | join(", ") }*/);

//## TODO: Inspect out structs
//## Check if the return code is a valid return code.
//## Leading false allows each generated entry to start with ||
//#     set chosen_ext_codes = gen.extensionReturnCodesForCommand(cur_cmd)
    bool recognizedReturnCode = (false
//## Core return codes
                /*%- for val in cur_cmd.return_values %*/ || /*{ checkResult(val) }*/ /*% endfor -%*/

//## Extension return codes, if any.
//## (These check that the extension is enabled.)
//#-            for ext_code in chosen_ext_codes
                || /*{ checkExtCode(ext_code) }*/
//#-            endfor
                );
    if (!recognizedReturnCode) {
        this->ConformanceFailure(XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT, /*{ cur_cmd.name | quote_string }*/, "Illegal result code returned: %d", result);
    }

//## If this is a create command, we have to create an entry in the appropriate
//## unordered_map pointing to the correct dispatch table for the newly created
//## object.
//#-        set is_last_arg_handle = (cur_cmd.params[-1].is_handle)
//#         set is_create = (("xrCreate" in cur_cmd.name) and is_last_arg_handle)
//#         if is_create
    if (XR_SUCCEEDED(result)) {
//#             set out_handle_name = cur_cmd.params[-1].name
//#             set out_handle_type = cur_cmd.params[-1].type
//#             set out_handle_object_type = gen.genXrObjectType(out_handle_type)
        // Normal "xrCreate" function: create and register state for child handle
        CreateAndRegisterHandleState(handleState,
            {HandleToInt(* /*{ out_handle_name }*/), /*{ out_handle_object_type }*/});
    }
//#         endif

//## Likewise, if it's a delete command, we have to remove the entry
//## for the dispatch table from the unordered_map
//#-        set is_destroy = (("xrDestroy" in cur_cmd.name) and is_last_arg_handle)
//#         if is_destroy
    if (XR_SUCCEEDED(result)) {
        // Normal "xrDestroy" function: unregister/destroy state for handle
        UnregisterHandleState(handleState);
    }
//#         endif


//## ### Special Case Handle Creation Follows ###
//## Dealing with other ways that handles come into our world

//## xrCreateSwapchainAndroidSurfaceKHR is a special case because
//## this is a create command but returned swapchain is not the last parameter.
//#         set is_create_swapchain_android_surface = ("xrCreateSwapchainAndroidSurfaceKHR" == cur_cmd.name)
//#         if is_create_swapchain_android_surface
    if (XR_SUCCEEDED(result)) {
//#             set out_handle_name = cur_cmd.params[-2].name
        CreateAndRegisterHandleState(handleState,
            {HandleToInt(* /*{ out_handle_name }*/), XR_OBJECT_TYPE_SWAPCHAIN});
    }
//#         endif

//## If this is a create command from an async api (EXT_future), we have to create an entry in the
//## appropriate unordered_map pointing to the correct dispatch table for the newly created object.
//#         set is_async_create = (("xrCreate" in cur_cmd.name) and ("Complete" in cur_cmd.name))
//#         if is_async_create and gen.isStruct(cur_cmd.params[-1].type)
    if (XR_SUCCEEDED(result)) {
//#             set last_param_name = cur_cmd.params[-1].name
//#             set last_param_struct = gen.getStruct(cur_cmd.params[-1].type)
//#             set completion_struct_last_param_name = last_param_struct.members[-1].name
//#             if last_param_struct.members[-1].is_handle and not last_param_struct.members[-1].is_array
//#                 set completion_struct_last_param_object_type = gen.genXrObjectType(last_param_struct.members[-1].type)
        CreateAndRegisterHandleState(handleState,
            {HandleToInt(/*{ last_param_name }*/->/*{ completion_struct_last_param_name }*/), /*{ completion_struct_last_param_object_type }*/});
//#             endif
    }
//#         endif

//## If this is a xrQuerySpacesFB, we have to create an entry in
//## the appropriate unordered_map pointing to the correct dispatch table for
//## the newly created objects.
//#-        set is_create_spatial_anchor = ("xrCreateSpatialAnchorFB" == cur_cmd.name)
//#         set is_query_spaces = ("xrQuerySpacesFB" == cur_cmd.name)
//#         if is_create_spatial_anchor or is_query_spaces
    if (XR_SUCCEEDED(result)) {
//#             set out_handle_name = cur_cmd.params[-1].name
        // Create "handle state" for the XrAsyncRequestIdFB value, with "object type" of the completion event expected.
        // The session is considered the parent handle.
        CreateAndRegisterHandleState(handleState,
            {* /*{ out_handle_name }*/, static_cast<XrObjectType>(XR_TYPE_EVENT_DATA_SPATIAL_ANCHOR_CREATE_COMPLETE_FB)});
    }
//#         endif

//## If this is a xrPollEvent and the event type returns an object, we have to
//## create an entry in the appropriate unordered_map pointing to the correct
//## dispatch table for the newly created object.
//#-        set is_pollevent = ("xrPollEvent" == cur_cmd.name)
//#         if is_pollevent
    if (XR_SUCCEEDED(result)) {
        if (eventData->type == XR_TYPE_EVENT_DATA_SPATIAL_ANCHOR_CREATE_COMPLETE_FB) {
            // Deal with new handle being returned in an async completion event.
            XrEventDataSpatialAnchorCreateCompleteFB* completeEvent = reinterpret_cast<XrEventDataSpatialAnchorCreateCompleteFB*>(eventData);
            // Lookup "handle state" for the XrAsyncRequestIdFB value, with "object type" of the event struct type.
            HandleState* const requestStateObject = GetHandleState({(IntHandle)completeEvent->requestId, static_cast<XrObjectType>(XR_TYPE_EVENT_DATA_SPATIAL_ANCHOR_CREATE_COMPLETE_FB)});
            CreateAndRegisterHandleState(requestStateObject->parent, // session: parent of request ID
                {HandleToInt(completeEvent->space), XR_OBJECT_TYPE_SPACE});
        }
    }
//#         endif

//## If this is a xrRetrieveSpaceQueryResultsFB, we have to create an entry in
//## the appropriate unordered_map pointing to the correct dispatch table for
//## the newly created objects.
//#-        set is_space_query_results = ("xrRetrieveSpaceQueryResultsFB" == cur_cmd.name)
//#         if is_space_query_results
    if (XR_SUCCEEDED(result)) {
//#             set out_param_name = cur_cmd.params[-1].name
        // Deal with created space handles
        if (/*{ out_param_name }*/->results) {
            for (uint32_t i = 0; i < /*{ out_param_name }*/->resultCountOutput; ++i) {
                CreateAndRegisterHandleState(handleState,
                    {HandleToInt(/*{ out_param_name }*/->results[i].space), XR_OBJECT_TYPE_SPACE});
            }
        }
    }
//#         endif

    return result;
}

/*{ protect_end(cur_cmd) }*/

//#     endif
//# endfor


static PFN_xrVoidFunction ConformanceLayer_InnerGetInstanceProcAddr(
    const char*                                 name,
    HandleState*                                handleState) {

    if (strcmp(name, "xrGetInstanceProcAddr") == 0) {
        return reinterpret_cast<PFN_xrVoidFunction>(ConformanceLayer_xrGetInstanceProcAddr);
    }
//# for cur_cmd in sorted_cmds
//#     set is_core = "XR_VERSION_" in cur_cmd.ext_name
//#     set is_core_1_0 = "XR_VERSION_1_0" in cur_cmd.ext_name
//#     if cur_cmd.name not in skip_hooks and cur_cmd.name != "xrGetInstanceProcAddr"

/*{ protect_begin(cur_cmd) }*/
    if (strcmp(name, /*{cur_cmd.name | quote_string}*/) == 0) {
//#         if not is_core
        if (handleState->conformanceHooks->enabledExtensions./*{cur_cmd.ext_name | make_ext_variable_name}*/) {
//#         elif not is_core_1_0
        if (handleState->conformanceHooks->enabledVersions./*{cur_cmd.ext_name | make_version_variable_name}*/) {
//#         endif
            return reinterpret_cast<PFN_xrVoidFunction>(ConformanceLayer_/*{cur_cmd.name}*/);
//#         if not is_core_1_0
        }
        return nullptr;
//#         endif
    }
/*{ protect_end(cur_cmd) }*/
//#     endif
//# endfor
    return nullptr;
}

XRAPI_ATTR XrResult XRAPI_CALL ConformanceLayer_xrGetInstanceProcAddr(
    XrInstance                                  instance,
    const char*                                 name,
    PFN_xrVoidFunction*                         function) try {

    if (instance == XR_NULL_HANDLE) {
        *function = nullptr;
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    }

    HandleState* const handleState = GetHandleState({ HandleToInt(instance), XR_OBJECT_TYPE_INSTANCE });

    *function = ConformanceLayer_InnerGetInstanceProcAddr(name, handleState);

    if (*function != nullptr) {
        return XR_SUCCESS;
    }

    // We have not found it, so pass it down to the next layer/runtime
    return handleState->conformanceHooks->dispatchTable.GetInstanceProcAddr(instance, name, function);
}
ABI_CATCH
