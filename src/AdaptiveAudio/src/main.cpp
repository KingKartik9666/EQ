#include "adaptive_audio/app/controller.h"

#include <iostream>
#include <string_view>

namespace {

void PrintUsage() {
    std::wcout << L"Usage: AdaptiveAudio.exe --status\n";
}

} // namespace

int wmain(const int argument_count, wchar_t* arguments[]) {
    adaptive_audio::app::Controller controller;
    if (!controller.Initialize()) {
        std::wcerr << L"AdaptiveAudio controller initialization failed.\n";
        return 1;
    }

    if (argument_count != 2 || std::wstring_view(arguments[1]) != L"--status") {
        PrintUsage();
        return 2;
    }

    std::wcout << L"AdaptiveAudio tray/controller scaffold is ready.\n"
               << L"Tray integration and engine IPC are not implemented yet.\n";
    controller.Stop();
    return 0;
}
