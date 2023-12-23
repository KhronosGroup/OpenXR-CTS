// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(XR_USE_PLATFORM_ANDROID)
// For Android, we require the following functions to be implemented
// in our library for accessing Android specific information.
void* Conformance_Android_Get_Application_VM();
void* Conformance_Android_Get_Application_Context();
void* Conformance_Android_Get_Application_Activity();
void* Conformance_Android_Get_Asset_Manager();
void Conformance_Android_Attach_Current_Thread();
void Conformance_Android_Detach_Current_Thread();
#endif  // defined(XR_USE_PLATFORM_ANDROID)
