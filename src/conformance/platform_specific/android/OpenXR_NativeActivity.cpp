// Copyright (c) 2019-2020 The Khronos Group Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>  // for memset
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>  // for prctl( PR_SET_NAME )
#include <sys/system_properties.h>
#include <thread>

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
/// We need to include all the EGL/GLES headers here or openxr_platform.h complains
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#define XR_USE_GRAPHICS_API_VULKAN 1
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

#define XR_USE_PLATFORM_ANDROID 1
/// For symmetry, also include all android headers before openxr_platform.h
#include <android/log.h>
#include <android/window.h>             // for AWINDOW_FLAG_KEEP_SCREEN_ON
#include <android/native_window_jni.h>  // for native window JNI
#include <android_native_app_glue.h>

/// Finally add OpenXR
#include <openxr/openxr.h>
#include <openxr/openxr_oculus.h>
#include <openxr/openxr_platform.h>

/// The Conformance framework does include some of the above as well,
/// and needs both XR_USE_GRAPHICS_API_OPENGL_ES and XR_USE_PLATFORM_ANDROID defined
/// in this context to run the test. NOTE, potentially a vulkan version could be added.
#include "conformance_test.h"

/// #define DEBUG 1
#define OVR_LOG_TAG "OpenXR_Android_TestBed"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)

/*
================================================================================

Native Activity

================================================================================
*/

/// I≈ to the Conformace framework to create the Android extensions properly
static void* AndroidApplicationVM = NULL;
static void* AndroidApplicationActivity = NULL;
static void* AndroidApplicationNativeWindow = NULL;
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

void* Conformance_Android_Get_Application_NativeWindow()
{
    ALOGV("AndroidApplicationNativeWindow = %p", AndroidApplicationNativeWindow);
    return AndroidApplicationNativeWindow;
}

/**
 * Process the next main command.
 */
static bool exitApp = false;
static bool resumeApp = false;
static void app_handle_cmd(struct android_app* app, int32_t cmd)
{
    /// ovrApp * appState = (ovrApp *)app->userData;

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

int32_t app_handle_input(struct android_app* app, AInputEvent* event)
{
    const int type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_KEY) {
        const int keyCode = AKeyEvent_getKeyCode(event);
        const int action = AKeyEvent_getAction(event);
        return 1;  // we eat all other key events
    }
    else if (type == AINPUT_EVENT_TYPE_MOTION) {
        const int action = AKeyEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        const float x = AMotionEvent_getRawX(event, 0);
        const float y = AMotionEvent_getRawY(event, 0);
        return 1;  // we eat all touch events
    }
    return 0;
}

#define OVR_LOG_PASSING_TESTS 0  /// change this to see each assertion

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

    /// Set these early on so that they are available to all tests
    AndroidApplicationVM = app->activity->vm;
    AndroidApplicationActivity = app->activity->clazz;

    // TODO: We should make this not required for OOPC apps.
    ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

    JNIEnv* Env;
    app->activity->vm->AttachCurrentThread(&Env, nullptr);

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);

    /// Hook up android handlers
    app->onAppCmd = app_handle_cmd;
    app->onInputEvent = app_handle_input;

    /// Initialize the loader for this platform
    XrLoaderInitializeInfoAndroidOCULUS loaderInitializeInfoAndroid;
    memset(&loaderInitializeInfoAndroid, 0, sizeof(loaderInitializeInfoAndroid));
    loaderInitializeInfoAndroid.type = XR_TYPE_LOADER_INITIALIZE_INFO_ANDROID_OCULUS;
    loaderInitializeInfoAndroid.next = NULL;
    loaderInitializeInfoAndroid.applicationVM = app->activity->vm;
    loaderInitializeInfoAndroid.applicationActivity = app->activity->clazz;
    xrInitializeLoaderOCULUS(&loaderInitializeInfoAndroid);

    /// Testing exception handling - needed for the conformance tests
    try {
        ALOGV("### Exception Test: - before throw...");
        throw std::runtime_error("### Exception Test DONE ###");
        ALOGV("### Exception Test: - after throw (should not hit!!)");
    }
    catch (const std::exception& e) {
        ALOGV("### Exception Test: caught - `%s`", e.what());
    }

    // Determine what Graphics API to test
    std::string graphicsApiStr;
    char value[PROP_VALUE_MAX] = {};
    if (__system_property_get("debug.xr.conformance_gfxapi", value) != 0) {
        graphicsApiStr = value;
        ALOGV("debug.xr.conformance_gfxapi = %s", graphicsApiStr.c_str());
    }

    if (graphicsApiStr.empty() || (graphicsApiStr != "OpenGLES" && graphicsApiStr != "Vulkan")) {
        graphicsApiStr = "OpenGLES";
    }
    ALOGV("Graphics API specified: %s", graphicsApiStr.c_str());

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
                androidTestThread = std::thread([=]() {
                    ALOGV("... begin conformance test ...");

                    /// Hard-Code these to match regular C declaration of `int main( int argc, char * argv[] )`
                    const char* argv[] = {
                        "OpenXR_Conformance_Test_Android",  /// app name
                        "-G",
                        graphicsApiStr.c_str(),  /// required: graphics plugin specifier
#if OVR_LOG_PASSING_TESTS
                        "-s",  /// include successful tests in output
#endif
                        "--use-colour",
                        "no",  /// no console coloring
                        "--reporter",
                        "console",  /// use the console reporter
                        NULL
                    };

#if OVR_LOG_PASSING_TESTS
                    int argc = 8;
#else
                    int argc = 7;
#endif
                    Conformance::Run(argc, const_cast<char**>(argv));

                    ALOGV("... end conformance test ...");

                    /// give the logger some time to flush
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));

                    /// call JNI exit instead
                    exitApp = true;
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
    jclass processClass = Env->FindClass("android.os.Process");
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
