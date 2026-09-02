#pragma once

#include "adaptive_audio/dsp/spsc_ring_buffer.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <type_traits>

namespace adaptive_audio::dsp {

// Precomputed RBJ biquad coefficients for one filter stage.
// Direct Form I:
//   y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
//
// The control coordinator validates finiteness and computes these off the
// real-time path. The audio callback only reads them.
struct BiquadCoefficients final {
    float b0{1.0f};
    float b1{0.0f};
    float b2{0.0f};
    float a1{0.0f};
    float a2{0.0f};

    // True only when all five coefficients are finite.
    [[nodiscard]] bool IsFinite() const noexcept {
        return std::isfinite(b0) && std::isfinite(b1) && std::isfinite(b2) &&
               std::isfinite(a1) && std::isfinite(a2);
    }
};

static_assert(std::is_trivially_copyable_v<BiquadCoefficients>);

// Three-band EQ packet: low shelf, presence (peak/bell), high shelf.
// Must be trivially copyable for SPSC delivery.
// The coordinator publishes this; the EqProcessor consumes it nonblockingly.
struct DspControlPacket final {
    BiquadCoefficients low_shelf{};
    BiquadCoefficients presence{};
    BiquadCoefficients high_shelf{};
    // Monotonically increasing sequence number (coordinator-assigned).
    // The processor accepts any packet with valid == true; sequence is for
    // future diagnostic use only.
    std::uint32_t sequence{0U};
    // false = coordinator requests bypass / neutral passthrough.
    bool valid{false};
};

static_assert(std::is_trivially_copyable_v<DspControlPacket>);

// Fixed-capacity SPSC queue delivering DspControlPackets from the control
// coordinator to the EqProcessor. Four slots (three usable) is sufficient
// because the coordinator writes infrequently (at most a few times per second).
inline constexpr std::size_t kDspControlQueueSlots{4U};
using DspControlQueue = SpscRingBuffer<DspControlPacket, kDspControlQueueSlots>;

// Returns a unity-gain (identity) packet. Used for startup and fallback.
[[nodiscard]] inline constexpr DspControlPacket NeutralDspControlPacket() noexcept {
    DspControlPacket p{};
    p.valid        = true;
    p.low_shelf    = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    p.presence     = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    p.high_shelf   = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    return p;
}

} // namespace adaptive_audio::dsp
