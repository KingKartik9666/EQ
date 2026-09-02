#pragma once

#include "adaptive_audio/foundation/log_severity.h"
#include "adaptive_audio/foundation/realtime_event.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>
#include <thread>

namespace adaptive_audio::foundation {

inline constexpr std::size_t kMaximumLogMessageCharacters{512U};

struct LoggerConfiguration final {
    std::filesystem::path file_path{};
    LogSeverity minimum_severity{LogSeverity::Info};
    std::size_t maximum_pending_records{256U};
    std::uint64_t maximum_file_size_bytes{4U * 1024U * 1024U};
};

struct AsyncLoggerMetrics final {
    std::atomic<std::uint64_t> accepted_records{0U};
    std::atomic<std::uint64_t> dropped_records{0U};
    std::atomic<std::uint64_t> written_records{0U};
    std::atomic<std::uint64_t> consumed_realtime_events{0U};
    std::atomic<std::uint64_t> suppressed_by_file_limit{0U};
    std::atomic<std::uint64_t> file_write_failures{0U};
};

// AsyncLogger is control-plane-only. Start, Stop, and Log may allocate, lock,
// wait, create directories, and perform filesystem I/O. The audio callback
// must communicate only through RealtimeEventSink, which has no logger link.
class AsyncLogger final {
public:
    AsyncLogger() = default;
    ~AsyncLogger();

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    // Non-real-time only. The logger owns one SPSC consumer if realtime_events
    // is non-null; no other thread may pop from that queue while it is running.
    [[nodiscard]] bool Start(const LoggerConfiguration& configuration,
                             RealtimeEventQueue* realtime_events = nullptr);
    void Stop() noexcept;

    // Non-real-time only. Message text is truncated to a fixed bound before it
    // enters the bounded, mutex-protected control-plane queue.
    [[nodiscard]] bool Log(LogSeverity severity, std::string_view message) noexcept;

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] const AsyncLoggerMetrics& Metrics() const noexcept;

private:
    struct QueuedLogRecord final {
        LogSeverity severity{LogSeverity::Info};
        std::array<char, kMaximumLogMessageCharacters> message{};
        std::uint16_t message_length{0U};
    };

    [[nodiscard]] static bool IsConfigurationValid(const LoggerConfiguration& configuration) noexcept;
    [[nodiscard]] static QueuedLogRecord MakeRecord(LogSeverity severity,
                                                     std::string_view message) noexcept;
    void WorkerMain() noexcept;
    void DrainRealtimeEvents() noexcept;
    void WriteRecord(const QueuedLogRecord& record) noexcept;
    [[nodiscard]] QueuedLogRecord MakeRealtimeEventRecord(const RealtimeEvent& event) const noexcept;

    mutable std::mutex mutex_;
    std::condition_variable wake_worker_;
    std::deque<QueuedLogRecord> pending_records_;
    std::thread worker_;
    std::ofstream output_;
    LoggerConfiguration configuration_{};
    RealtimeEventQueue* realtime_events_{nullptr};
    std::uint64_t file_bytes_written_{0U};
    bool accepting_records_{false};
    bool stop_requested_{false};
    AsyncLoggerMetrics metrics_{};
};

} // namespace adaptive_audio::foundation
