#include <iostream>
#include <string_view>

namespace {

void PrintUsage() {
    std::wcout << L"Usage: AudioBenchmarks.exe --help\n"
               << L"\n"
               << L"This Phase 1 scaffold does not open an audio endpoint or report measurements.\n"
               << L"It provides metric storage for a future measured PCM path only.\n";
}

} // namespace

int wmain(const int argument_count, wchar_t* arguments[]) {
    if (argument_count == 2 && std::wstring_view(arguments[1]) == L"--help") {
        PrintUsage();
        return 0;
    }

    PrintUsage();
    return 2;
}
