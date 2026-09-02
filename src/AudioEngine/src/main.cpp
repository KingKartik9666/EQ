#include "adaptive_audio/windows/endpoint_enumerator.h"

#include <windows.h>

#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

void PrintUsage() {
    std::wcout << L"Usage: AudioEngine.exe --list-render-endpoints\n";
}

void PrintEndpoint(const adaptive_audio::windows::RenderEndpointInfo& endpoint) {
    std::wcout << L"Name: " << endpoint.friendly_name << L'\n'
               << L"  ID: " << endpoint.device_id << L'\n'
               << L"  Mix format: " << endpoint.sample_rate_hz << L" Hz, "
               << endpoint.channel_count << L" channel(s), " << endpoint.bits_per_sample
               << L" bits, " << (endpoint.float_pcm ? L"float PCM" : L"non-float PCM")
               << L", mask 0x" << std::hex << endpoint.channel_mask << std::dec << L"\n";
}

} // namespace

int wmain(const int argument_count, wchar_t* arguments[]) {
    if (argument_count != 2 || std::wstring_view(arguments[1]) != L"--list-render-endpoints") {
        PrintUsage();
        return 2;
    }

    const HRESULT initialize_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialize_result)) {
        std::wcerr << L"COM initialization failed: 0x" << std::hex
                   << static_cast<unsigned long>(initialize_result) << L'\n';
        return 1;
    }

    std::vector<adaptive_audio::windows::RenderEndpointInfo> endpoints;
    const long enumerate_result =
        adaptive_audio::windows::EnumerateActiveRenderEndpoints(endpoints);
    CoUninitialize();

    if (FAILED(enumerate_result)) {
        std::wcerr << L"Endpoint enumeration failed: 0x" << std::hex
                   << static_cast<unsigned long>(enumerate_result) << L'\n';
        return 1;
    }

    for (const auto& endpoint : endpoints) {
        PrintEndpoint(endpoint);
    }
    return 0;
}
