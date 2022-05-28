// Copyright (c) 2019-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PATH_PREFIX
#define PATH_PREFIX "/sdcard/"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>  // for memset
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/system_properties.h>
#include <thread>
#include <vector>

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#define XR_USE_GRAPHICS_API_VULKAN 1
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

#define XR_USE_PLATFORM_ANDROID 1
#include <android/log.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <conformance_test.h>
#include <conformance_framework.h>

#include "conformance_framework.h"

/// #define DEBUG 1
#define LOG_TAG "OpenXR_Conformance"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)

/*
================================================================================

Native Activity

================================================================================
*/

// Required for android create instance extension.
static JavaVM* AndroidApplicationVM = NULL;
static jobject AndroidApplicationActivity = NULL;
static void* AndroidApplicationNativeWindow = NULL;
static AAssetManager* AndroidAssetManager = NULL;
void* Conformance_Android_Get_Application_VM()
{
    ALOGV("AndroidApplicationVM = %p", AndroidApplicationVM);
    return AndroidApplicationVM;
}

void* Conformance_Android_Get_Application_Activity()
{
    ALOGV("AndroidApplicationActivity = %p", AndroidApplicationActivity);
    return AndroidApplicationActivity;
}

void* Conformance_Android_Get_Asset_Manager()
{
    ALOGV("AndroidAssetManager = %p", AndroidAssetManager);
    return AndroidAssetManager;
}

void Conformance_Android_Attach_Current_Thread()
{
    ALOGV("AttachCurrentThread");
    JNIEnv* Env;
    AndroidApplicationVM->AttachCurrentThread(&Env, nullptr);
}

void Conformance_Android_Detach_Current_Thread()
{
    ALOGV("DetachCurrentThread");
    AndroidApplicationVM->DetachCurrentThread();
}

/**
 * Process the next main command.
 */
static bool exitApp = false;
static bool resumeApp = false;
static void app_handle_cmd(struct android_app* app, int32_t cmd)
{
    switch (cmd) {
    // There is no APP_CMD_CREATE. The ANativeActivity creates the
    // application thread from onCreate(). The application thread
    // then calls android_main().
    case APP_CMD_START: {
        ALOGV("    APP_CMD_START");
        break;
    }
    case APP_CMD_RESUME: {
        ALOGV("    APP_CMD_RESUME");
        resumeApp = true;
        break;
    }
    case APP_CMD_PAUSE: {
        ALOGV("    APP_CMD_PAUSE");
        resumeApp = false;
        break;
    }
    case APP_CMD_STOP: {
        ALOGV("    APP_CMD_STOP");
        break;
    }
    case APP_CMD_DESTROY: {
        ALOGV("    APP_CMD_DESTROY");
        exitApp = true;
        break;
    }
    case APP_CMD_INIT_WINDOW: {
        ALOGV("    APP_CMD_INIT_WINDOW");
        AndroidApplicationNativeWindow = app->window;
        break;
    }
    case APP_CMD_TERM_WINDOW: {
        ALOGV("    APP_CMD_TERM_WINDOW");
        AndroidApplicationNativeWindow = NULL;
        break;
    }
    }
}

int32_t app_handle_input(struct android_app* /* app */, AInputEvent* event)
{
    const int type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        AKeyEvent_getKeyCode(event);
        AKeyEvent_getAction(event);
        return 1;  // we eat all other key events
    }
    else if (type == AINPUT_EVENT_TYPE_MOTION) {
        AKeyEvent_getAction(event);
        AMotionEvent_getRawX(event, 0);
        AMotionEvent_getRawY(event, 0);
        return 1;  // we eat all touch events
    }
    return 0;
}

XRAPI_ATTR void XRAPI_CALL OnTestMessage(MessageType type, const char* message)
{
    switch (type) {
    case MessageType_Stdout:
    case MessageType_Stderr:
    case MessageType_AssertionFailed:
    case MessageType_TestSectionStarting:
        ALOGV("%s", message);
        break;
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* app)
{
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");

    // Set these early on so that they are available to all tests
    AndroidApplicationVM = app->activity->vm;
    AndroidApplicationActivity = app->activity->clazz;

    JNIEnv* Env;
    app->activity->vm->AttachCurrentThread(&Env, nullptr);

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);

    AndroidAssetManager = app->activity->assetManager;

    // Hook up android handlers
    app->onAppCmd = app_handle_cmd;
    app->onInputEvent = app_handle_input;

    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
    if (XR_SUCCEEDED(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)(&xrInitializeLoaderKHR))) &&
        xrInitializeLoaderKHR != NULL) {
        XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid;
        memset(&loaderInitializeInfoAndroid, 0, sizeof(loaderInitializeInfoAndroid));
        loaderInitializeInfoAndroid.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
        loaderInitializeInfoAndroid.next = NULL;
        loaderInitializeInfoAndroid.applicationVM = app->activity->vm;
        loaderInitializeInfoAndroid.applicationContext = app->activity->clazz;
        xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid);
    }

    // Testing exception handling - needed for the conformance tests
    try {
        ALOGV("### Exception Test: - before throw...");
        throw std::runtime_error("### Exception Test DONE ###");
        ALOGV("### Exception Test: - after throw (should not hit!!)");
    }
    catch (const std::exception& e) {
        ALOGV("### Exception Test: caught - `%s`", e.what());
    }

    char argstr[PROP_VALUE_MAX] = {};
    if (__system_property_get("debug.xr.conform.args", argstr) != 0) {
        ALOGV("debug.xr.conform.args: %s", argstr);
    }

    exitApp = false;
    bool testThreadStarted = false;
    std::thread androidTestThread;

    // main loop to wait for window and other resource initialization
    while (app->destroyRequested == 0) {
        int events;
        struct android_poll_source* source = nullptr;
        const int timeoutMilliseconds = 0;
        if (ALooper_pollAll(timeoutMilliseconds, nullptr, &events, (void**)&source) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }

            /// Run the actual conformance tests only when all required android components are present
            if (testThreadStarted == false && AndroidApplicationNativeWindow != NULL) {
                testThreadStarted = true;
                androidTestThread = std::thread([&]() {
                    ATTACH_THREAD;
                    ALOGV("... begin conformance test ...");

                    /// Hard-Code these to match regular C declaration of `int main( int argc, char * argv[] )`
                    std::vector<const char*> args = {
                        "OpenXR_Conformance_Test_Android",  /// app name
                    };
                    char* append = argstr;
                    int count = 1;

                    bool reportXml = false;

                    while (*append != 0) {
                        while (*append == ' ') {
                            append++;
                        }
                        if (*append != 0) {
                            if (strcmp(append, "-O") == 0) {
                                reportXml = true;
                            }
                            else {
                                args.insert(args.begin() + count, append);
                                count++;
                            }
                        }
                        while (*append != ' ' && *append != 0) {
                            append++;
                        }
                        if (*append == ' ') {
                            *append = 0;
                            append++;
                        }
                    }
                    bool hasGfxFlag = false;
                    for (int i = 0; i < (int)args.size(); i++) {
                        std::string s = args[i];
                        if (s == "-G") {
                            hasGfxFlag = true;
                            break;
                        }
                    }
                    if (hasGfxFlag == false) {
                        args.push_back("-G");
                        args.push_back("OpenGLES");
                    }

                    if (reportXml) {
                        args.push_back("-r");
                        args.push_back("xml");
                        args.push_back("-o");
                        args.push_back(PATH_PREFIX "openxr_conformance.xml");
                    }
                    else {
                        args.push_back("--use-colour");
                        args.push_back("no");  /// no console coloring
                        args.push_back("--reporter");
                        args.push_back("console");  /// use the console reporter
                    }

                    for (int i = 0; i < (int)args.size(); i++) {
                        ALOGV("arg[%d] = %s", i, args[i]);
                    }

                    ConformanceLaunchSettings launchSettings;
                    launchSettings.argc = args.size();
                    launchSettings.argv = (&args[0]);
                    launchSettings.message = OnTestMessage;

                    uint32_t failureCount = 0;
                    XrcResult result = xrcRunConformanceTests(&launchSettings, &failureCount);
                    const int exitResult = result == XRC_SUCCESS ? 0 : std::max((uint32_t)1, failureCount);
                    ALOGV("Exit Result %d", exitResult);

                    ALOGV("... end conformance test ...");

                    // give the logger some time to flush
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));

                    // call JNI exit instead
                    exitApp = true;
#ifdef BUILD_FOR_FB
                    // Only needed for facebook runtime
                    DETACH_THREAD;
#endif
                });
            }
        }

        if (exitApp) {
            exitApp = false;
            ALOGV("... calling ANativeActivity_finish ...");
            ANativeActivity_finish(app->activity);
        }
    }

    ALOGV("... joining test thread ...");
    if (androidTestThread.joinable()) {
        androidTestThread.join();
    }

    /// give the logger some time to flush
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /// Destroy this process so that catch2 globals can be clean again on relaunch
    jclass processClass = Env->FindClass("android/os/Process");
    ALOGV("... processClass = %p", processClass);
    jmethodID myPidMethodId = Env->GetStaticMethodID(processClass, "myPid", "()I");
    ALOGV("... myPidMethodId = %p", myPidMethodId);
    jmethodID killProcessMethodId = Env->GetStaticMethodID(processClass, "killProcess", "(I)V");
    ALOGV("... killProcessMethodId = %p", killProcessMethodId);
    jint pid = Env->CallStaticIntMethod(processClass, myPidMethodId);
    ALOGV("... pid = %d", pid);
    Env->CallStaticVoidMethod(processClass, killProcessMethodId, pid);

    /// This should never execute
    ALOGV("... detaching Java VM thread ...");
    app->activity->vm->DetachCurrentThread();
    ALOGV("    android_main() DONE");
    ALOGV("android_app_entry() DONE");
    ALOGV("----------------------------------------------------------------");
}
