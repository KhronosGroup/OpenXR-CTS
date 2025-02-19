// Copyright (c) 2019-2025 The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
#include "android_intent_extras.h"
#include <android_native_app_glue.h>

#include "jnipp/jnipp.h"

#include <android/log.h>
#include <string>

#define LOG_TAG "OpenXR_Conformance"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)

// We first check for a string array intent extra named this
static constexpr const char* kStringArrayExtraName = "args";

// Then, we check for array string intent extras with the following names, which match
// the names of command line options in the CLI: see `MakeCLIParser` for help.
static constexpr auto kStringArrayExtraNames = {
    "enabledInstanceExtension",
    "interactionProfiles",
};

// Then, we check for individual string intent extras with the following names, which match
// the names of command line options in the CLI: see `MakeCLIParser` for help.
static constexpr auto kStringExtraNames = {
    "graphicsPlugin", "apiVersion", "formFactor", "hands", "viewConfiguration", "environmentBlendMode",
};

// If we find a string extra with this name, its contents are used as the filename (within the application storage) to write.
static constexpr const char* kFilenameStringExtraName = "xmlFilename";

// Finally we check to see if XML output is requested to be skipped
static constexpr const char* kSkipXmlBooleanExtraName = "skipXml";

namespace Conformance
{
    IntentExtrasData parseIntentExtras(void* vm, void* activity)
    {
        IntentExtrasData ret;
        jni::init((JavaVM*)vm);

        jni::Object act{(jobject)activity};
        jni::Class activityClass("android/app/Activity");
        auto getIntent = activityClass.getMethod("getIntent", "()Landroid/content/Intent;");

        // activityClass.getMethod("getIntent", )
        jni::Object intent = act.call<jni::Object>(getIntent);

        jni::Class intentClass("android/content/Intent");
        {
            auto getStringArrayExtra = intentClass.getMethod("getStringArrayExtra", "(Ljava/lang/String;)[Ljava/lang/String;");
            // auto args = intent.call<jni::Array<jni::Object>>(getStringArrayExtra, kStringArrayExtraName);
            auto args = intent.call<jni::Array<std::string>>(getStringArrayExtra, kStringArrayExtraName);
            if (!args.isNull()) {
                // jnipp does not have iterators for java arrays so no range-for
                const long n = args.getLength();
                ALOGV("Got a string array intent extras of size %ld", n);
                for (long i = 0; i < n; ++i) {
                    // Passing multiple elements in here works as follows:
                    // --esa args element1,element2
                    // this will show up as a 2 element string array.
                    //
                    // If you wish to pass in a list of OR'd tests they must be comma separated.
                    // however Android treats the "," as a special character and recommends escaping
                    // the comma using a backslash: \,
                    //
                    // Unfortunately Android does not unescape before passing the data along, confusing
                    // Catch2. Hence we unescape here ourselves.
                    // https://android.googlesource.com/platform/frameworks/base/+/21bdaf1/cmds/am/src/com/android/commands/am/Am.java#581
                    //
                    // The code below allows: --esa args InteractiveThrow\\,GripAndAimPose
                    // Which will run just the two tests above.
                    std::string argument = args[i];
                    size_t pos = 0;
                    while ((pos = argument.find("\\,")) != std::string::npos) {
                        argument.replace(pos, 2, ",");
                    }
                    ret.arguments.emplace_back(argument);
                }
            }

            // Example usage --esa enabledInstanceExtension XR_EXT_user_presence,XR_KHR_visibility_mask
            for (const char* name : kStringArrayExtraNames) {
                auto esa_args = intent.call<jni::Array<std::string>>(getStringArrayExtra, name);
                if (!esa_args.isNull()) {
                    // jnipp does not have iterators for java arrays so no range-for
                    const long n = esa_args.getLength();
                    ALOGV("Got a string array intent extras of size %ld for %s", n, name);
                    for (long i = 0; i < n; ++i) {
                        ALOGV("Adding option %s for %s", esa_args[i].c_str(), name);
                        ret.arguments.emplace_back(std::string("--") + name);
                        ret.arguments.emplace_back(esa_args[i]);
                    }
                }
            }
        }
        {
            auto getStringExtra = intentClass.getMethod("getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");

            for (const char* name : kStringExtraNames) {
                std::string result = intent.call<std::string>(getStringExtra, std::string(name));

                if (!result.empty()) {
                    // found it, push the corresponding arg to our fake argv
                    ALOGV("Found intent string extra for %s, mapping into option", name);
                    ret.arguments.emplace_back(std::string("--") + name);
                    ret.arguments.emplace_back(std::move(result));
                }
            }

            std::string filename = intent.call<std::string>(getStringExtra, kFilenameStringExtraName);
            if (!filename.empty()) {
                ALOGV("Found intent string extra for %s, recording custom XML output filename %s", kFilenameStringExtraName,
                      filename.c_str());
                ret.xmlFilename = filename;
            }
        }
        {
            auto getBooleanExtra = intentClass.getMethod("getBooleanExtra", "(Ljava/lang/String;Z)Z");
            // the parameter is for *skipping* XML, because we want to write it by default.
            ret.shouldAddXmlOutput = !intent.call<bool>(getBooleanExtra, kSkipXmlBooleanExtraName, false);
        }
        return ret;
    }
}  // namespace Conformance
