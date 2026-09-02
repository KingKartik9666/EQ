#include "adaptive_audio/audio_service/per_user_audio_host.h"

#include <iostream>
#include <string_view>

namespace {

void PrintUsage() {
    std::wcout << L"Usage: AudioService.exe --status\n";
}

} // namespace

int wmain(const int argument_count, wchar_t* arguments[]) {
    if (argument_count != 2 || std::wstring_view(arguments[1]) != L"--status") {
        PrintUsage();
        return 2;
    }

    adaptive_audio::audio_service::PerUserAudioHost host;
    if (!host.InitializeForCurrentProcess()) {
        std::wcerr << L"Unable to resolve the current Windows session.\n";
        return 1;
    }

    const auto& status = host.status();
    std::wcout << L"AudioService per-user host scaffold is ready.\n"
               << L"Current session: " << status.session_id << L'\n'
               << L"Session 0: " << (status.is_session_zero ? L"yes" : L"no") << L'\n'
               << L"Controlled PCM path: "
               << (status.controlled_audio_path_active ? L"active" : L"not implemented")
               << L'\n';
    return 0;
}
