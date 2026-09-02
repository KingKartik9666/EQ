#include "adaptive_audio/audio_service/per_user_audio_host.h"

#include <windows.h>

namespace adaptive_audio::audio_service {

bool PerUserAudioHost::InitializeForCurrentProcess() noexcept {
    DWORD session_id{};
    if (::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id) == FALSE) {
        status_ = {};
        return false;
    }

    status_.session_id = session_id;
    status_.current_session_resolved = true;
    status_.is_session_zero = session_id == 0U;
    // Audio routing begins only once a real virtual-endpoint/PCM bridge exists.
    status_.controlled_audio_path_active = false;
    return true;
}

const PerUserHostStatus& PerUserAudioHost::status() const noexcept {
    return status_;
}

} // namespace adaptive_audio::audio_service
