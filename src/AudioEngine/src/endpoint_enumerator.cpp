#include "adaptive_audio/windows/endpoint_enumerator.h"

#include <windows.h>

#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <propvarutil.h>
#include <wrl/client.h>

#include <memory>
#include <new>
#include <utility>

namespace adaptive_audio::windows {
namespace {

using Microsoft::WRL::ComPtr;

[[nodiscard]] std::wstring ReadFriendlyName(IPropertyStore* const store) {
    PROPVARIANT value;
    PropVariantInit(&value);
    const HRESULT result = store->GetValue(PKEY_Device_FriendlyName, &value);
    std::wstring name;
    if (SUCCEEDED(result) && value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        name = value.pwszVal;
    }
    PropVariantClear(&value);
    return name;
}

[[nodiscard]] HRESULT ReadMixFormat(IMMDevice* const device,
                                    RenderEndpointInfo& endpoint) noexcept {
    ComPtr<IAudioClient> audio_client;
    HRESULT result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                      reinterpret_cast<void**>(audio_client.GetAddressOf()));
    if (FAILED(result)) {
        return result;
    }

    WAVEFORMATEX* raw_format{nullptr};
    result = audio_client->GetMixFormat(&raw_format);
    if (FAILED(result)) {
        return result;
    }

    const std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> format(raw_format, CoTaskMemFree);
    endpoint.sample_rate_hz = format->nSamplesPerSec;
    endpoint.channel_count = format->nChannels;
    endpoint.bits_per_sample = format->wBitsPerSample;
    endpoint.float_pcm = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;

    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        format->cbSize >= (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))) {
        const auto* const extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format.get());
        endpoint.channel_mask = extensible->dwChannelMask;
        endpoint.float_pcm = extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        endpoint.bits_per_sample = extensible->Samples.wValidBitsPerSample;
    }
    return S_OK;
}

} // namespace

long EnumerateActiveRenderEndpoints(std::vector<RenderEndpointInfo>& endpoints) noexcept {
    endpoints.clear();

    try {
        ComPtr<IMMDeviceEnumerator> enumerator;
        HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                          __uuidof(IMMDeviceEnumerator),
                                          reinterpret_cast<void**>(enumerator.GetAddressOf()));
        if (FAILED(result)) {
            return result;
        }

        ComPtr<IMMDeviceCollection> devices;
        result = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                                devices.GetAddressOf());
        if (FAILED(result)) {
            return result;
        }

        UINT count{0U};
        result = devices->GetCount(&count);
        if (FAILED(result)) {
            return result;
        }
        endpoints.reserve(count);

        for (UINT index = 0U; index < count; ++index) {
            ComPtr<IMMDevice> device;
            result = devices->Item(index, device.GetAddressOf());
            if (FAILED(result)) {
                return result;
            }

            LPWSTR raw_id{nullptr};
            result = device->GetId(&raw_id);
            if (FAILED(result)) {
                return result;
            }
            const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> device_id(raw_id, CoTaskMemFree);

            ComPtr<IPropertyStore> property_store;
            result = device->OpenPropertyStore(STGM_READ, property_store.GetAddressOf());
            if (FAILED(result)) {
                return result;
            }

            RenderEndpointInfo endpoint{};
            endpoint.device_id = device_id.get();
            endpoint.friendly_name = ReadFriendlyName(property_store.Get());
            result = ReadMixFormat(device.Get(), endpoint);
            if (FAILED(result)) {
                return result;
            }
            endpoints.push_back(std::move(endpoint));
        }
        return S_OK;
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    } catch (...) {
        // This is a control-plane API boundary. Do not allow an unexpected C++
        // exception to cross it into the process host.
        return E_FAIL;
    }
}

} // namespace adaptive_audio::windows
