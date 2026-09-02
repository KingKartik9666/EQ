#include "adaptive_audio/dsp/audio_format.h"
#include "adaptive_audio/dsp/passthrough_processor.h"
#include "adaptive_audio/dsp/spsc_ring_buffer.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

[[noreturn]] void Fail(const std::string_view message) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

void Require(const bool condition, const std::string_view message) {
    if (!condition) {
        Fail(message);
    }
}

void TestSpscQueue() {
    adaptive_audio::dsp::SpscRingBuffer<std::uint32_t, 4U> queue;
    Require(queue.Capacity() == 3U, "queue capacity uses one sentinel slot");
    Require(queue.TryPush(1U), "push one");
    Require(queue.TryPush(2U), "push two");
    Require(queue.TryPush(3U), "push three");
    Require(!queue.TryPush(4U), "full queue rejects without overwrite");

    std::uint32_t value{0U};
    Require(queue.TryPop(value) && value == 1U, "first item is FIFO");
    Require(queue.TryPop(value) && value == 2U, "second item is FIFO");
    Require(queue.TryPop(value) && value == 3U, "third item is FIFO");
    Require(!queue.TryPop(value), "empty queue reports empty");
}

void TestTransparentPcmCopy() {
    adaptive_audio::dsp::PcmPassthroughProcessor processor;
    Require(processor.Configure({.sample_rate_hz = 48'000U,
                                 .channel_count = 2U,
                                 .channel_mask = 0x3U,
                                 .bytes_per_frame = 8U},
                                16U),
            "float32 stereo passthrough setup succeeds");

    const std::array<std::byte, 24U> input{
        std::byte{0x00}, std::byte{0x80}, std::byte{0x01}, std::byte{0x7F},
        std::byte{0x20}, std::byte{0x10}, std::byte{0xEF}, std::byte{0xAA},
        std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78},
        std::byte{0x9A}, std::byte{0xBC}, std::byte{0xDE}, std::byte{0xF0},
        std::byte{0xFF}, std::byte{0x00}, std::byte{0x11}, std::byte{0x22},
        std::byte{0x33}, std::byte{0x44}, std::byte{0x55}, std::byte{0x66},
    };
    std::array<std::byte, 24U> output{};
    adaptive_audio::dsp::PassthroughMetrics metrics;
    Require(processor.Process(input.data(), output.data(), 3U, metrics),
            "complete PCM frames are copied");
    Require(input == output, "passthrough changes no PCM byte");
    Require(metrics.processed_frames.load() == 3U, "frame count is exact");

    Require(!processor.Process(input.data(), output.data(), 17U, metrics),
            "oversized callback is rejected rather than becoming unbounded");
    Require(metrics.rejected_calls.load() == 1U, "rejection is measured");

    std::array<std::byte, 24U> in_place = input;
    Require(processor.Process(in_place.data(), in_place.data(), 3U, metrics),
            "in-place PCM passthrough is safe");
    Require(in_place == input, "in-place path preserves bytes");
}

void TestRejectedConfiguration() {
    adaptive_audio::dsp::PcmPassthroughProcessor processor;
    Require(!processor.Configure({.sample_rate_hz = 8'000U,
                                  .channel_count = 2U,
                                  .channel_mask = 0x3U,
                                  .bytes_per_frame = 8U},
                                 16U),
            "unsupported sample rate is rejected");
    Require(!processor.Configure({.sample_rate_hz = 48'000U,
                                  .channel_count = 2U,
                                  .channel_mask = 0x3U,
                                  .bytes_per_frame = 1U},
                                 16U),
            "impossible frame layout is rejected");
}

} // namespace

int main() {
    TestSpscQueue();
    TestTransparentPcmCopy();
    TestRejectedConfiguration();
    std::cout << "All DSP core tests passed.\n";
    return 0;
}
