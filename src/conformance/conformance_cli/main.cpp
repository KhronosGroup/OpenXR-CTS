// Copyright (c) 2019-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <algorithm>
#include <xr_dependencies.h>
#include <conformance_test.h>

#if defined(_WIN32)
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
// Favor the high performance NVIDIA or AMD GPUs
extern "C" {
// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// https://gpuopen.com/learn/amdpowerxpressrequesthighperformance/
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif  // defined(_WIN32)

namespace
{
    XRAPI_ATTR void XRAPI_CALL OnTestMessage(MessageType type, const char* message)
    {
        constexpr const char* ResetColor = "\033[0m";
        switch (type) {
        case MessageType_Stdout:
            std::cout << message;
            break;
        case MessageType_Stderr:
            std::cerr << message;
            break;
        case MessageType_AssertionFailed:
            std::cout << /* Red */ "\033[1;31m" << message << ResetColor;
            break;
        case MessageType_TestSectionStarting:
            std::cout << /* White */ "\033[1;37m" << message << ResetColor;
            break;
        }
    }

    void SetupConsole()
    {
#if defined(_WIN32)  // Enable ANSI style color escape codes on Windows. Not enabled by default :-(
        DWORD consoleMode;
        if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &consoleMode)) {
            consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), consoleMode);
        }
#endif
    }
}  // namespace

int main(int argc, const char** argv)
{
    SetupConsole();

    ConformanceLaunchSettings launchSettings;
    launchSettings.argc = argc;
    launchSettings.argv = argv;
    launchSettings.message = OnTestMessage;

    uint32_t failureCount = 0;
    XrcResult result = xrcRunConformanceTests(&launchSettings, &failureCount);
    if (result != XRC_SUCCESS) {
        return 2;  // Tests failed to run.
    }

    xrcCleanup();

    return failureCount == 0 ? 0 : 1;
}
