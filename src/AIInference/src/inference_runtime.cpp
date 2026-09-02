#include "adaptive_audio/inference/inference_runtime.h"

namespace adaptive_audio::inference {

RuntimeCapabilities InferenceRuntime::capabilities() const noexcept {
    return {};
}

} // namespace adaptive_audio::inference
