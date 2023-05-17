// Copyright (c) 2019-2023, The Khronos Group Inc.
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

// Then, we check for individual string intent extras with the following names, which match
// the names of command line options in the CLI: see `MakeCLIParser` for help.
static constexpr auto kStringExtraNames = {
    "graphicsPlugin", "formFactor", "hands", "viewConfiguration", "environmentBlendMode",
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
                    ret.arguments.emplace_back(args[i]);
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
