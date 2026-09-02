#include "adaptive_audio/foundation/async_logger.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>

namespace adaptive_audio::foundation {

namespace {

// Returns an ISO-8601-like UTC timestamp string suitable for log lines.
// Runs on the worker thread (non-real-time). May allocate.
[[nodiscard]] std::string CurrentTimestamp() noexcept {
    try {
        const auto now = std::chrono::system_clock::now();
        const auto now_t = std::chrono::system_clock::to_time_t(now);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) %
                        1000;
        std::tm utc{};
#if defined(_WIN32)
        gmtime_s(&utc, &now_t);
#else
        gmtime_r(&now_t, &utc);
#endif
        return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
                           utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                           utc.tm_hour, utc.tm_min, utc.tm_sec,
                           static_cast<int>(ms.count()));
    } catch (...) {
        return "1970-01-01T00:00:00.000Z";
    }
}

} // namespace

AsyncLogger::~AsyncLogger() {
    Stop();
}

bool AsyncLogger::Start(const LoggerConfiguration& configuration,
                        RealtimeEventQueue* const realtime_events) {
    if (!IsConfigurationValid(configuration)) {
        return false;
    }

    std::lock_guard lock(mutex_);
    if (accepting_records_) {
        return false; // already running
    }

    // Create parent directories (control-plane, may throw).
    try {
        const auto parent = configuration.file_path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
    } catch (...) {
        return false;
    }

    output_.open(configuration.file_path, std::ios::out | std::ios::app);
    if (!output_.is_open()) {
        return false;
    }

    // Determine existing file size so we respect the byte limit across restarts.
    output_.seekp(0, std::ios::end);
    const auto pos = output_.tellp();
    file_bytes_written_ = (pos == static_cast<std::streampos>(-1))
                              ? 0U
                              : static_cast<std::uint64_t>(pos);

    configuration_ = configuration;
    realtime_events_ = realtime_events;
    stop_requested_ = false;
    accepting_records_ = true;

    worker_ = std::thread([this] { WorkerMain(); });
    return true;
}

void AsyncLogger::Stop() noexcept {
    {
        std::lock_guard lock(mutex_);
        if (!accepting_records_) {
            return;
        }
        accepting_records_ = false;
        stop_requested_ = true;
    }
    wake_worker_.notify_one();

    if (worker_.joinable()) {
        worker_.join();
    }
    if (output_.is_open()) {
        output_.close();
    }
}

bool AsyncLogger::Log(const LogSeverity severity, const std::string_view message) noexcept {
    if (!MeetsMinimumSeverity(severity, configuration_.minimum_severity)) {
        return true; // filtered, not an error
    }

    try {
        std::lock_guard lock(mutex_);
        if (!accepting_records_) {
            return false;
        }
        if (pending_records_.size() >= configuration_.maximum_pending_records) {
            metrics_.dropped_records.fetch_add(1U, std::memory_order_relaxed);
            return false;
        }
        pending_records_.push_back(MakeRecord(severity, message));
        metrics_.accepted_records.fetch_add(1U, std::memory_order_relaxed);
    } catch (...) {
        metrics_.dropped_records.fetch_add(1U, std::memory_order_relaxed);
        return false;
    }

    wake_worker_.notify_one();
    return true;
}

bool AsyncLogger::IsRunning() const noexcept {
    std::lock_guard lock(mutex_);
    return accepting_records_;
}

const AsyncLoggerMetrics& AsyncLogger::Metrics() const noexcept {
    return metrics_;
}

// ----- Private -----

bool AsyncLogger::IsConfigurationValid(const LoggerConfiguration& configuration) noexcept {
    return !configuration.file_path.empty() &&
           configuration.maximum_pending_records >= 1U &&
           configuration.maximum_file_size_bytes >= 1U;
}

AsyncLogger::QueuedLogRecord AsyncLogger::MakeRecord(const LogSeverity severity,
                                                      const std::string_view message) noexcept {
    QueuedLogRecord record{};
    record.severity = severity;
    const std::size_t copy_len =
        std::min(message.size(), kMaximumLogMessageCharacters - 1U);
    std::memcpy(record.message.data(), message.data(), copy_len);
    record.message_length = static_cast<std::uint16_t>(copy_len);
    return record;
}

void AsyncLogger::WorkerMain() noexcept {
    for (;;) {
        std::unique_lock lock(mutex_);
        // Wait at most 50 ms so we periodically drain the real-time event queue.
        wake_worker_.wait_for(lock, std::chrono::milliseconds(50), [this] {
            return stop_requested_ || !pending_records_.empty();
        });

        const bool stopping = stop_requested_;

        // Drain control-plane queue under the lock, writing one record at a time
        // after releasing it so we do not hold the lock during I/O.
        while (!pending_records_.empty()) {
            QueuedLogRecord record = pending_records_.front();
            pending_records_.pop_front();
            lock.unlock();
            WriteRecord(record);
            lock.lock();
        }
        lock.unlock();

        // Drain real-time events (SPSC pop — no lock required).
        DrainRealtimeEvents();

        if (stopping) {
            break;
        }
    }
}

void AsyncLogger::DrainRealtimeEvents() noexcept {
    if (realtime_events_ == nullptr) {
        return;
    }
    RealtimeEvent event{};
    while (realtime_events_->TryPop(event)) {
        metrics_.consumed_realtime_events.fetch_add(1U, std::memory_order_relaxed);
        WriteRecord(MakeRealtimeEventRecord(event));
    }
}

void AsyncLogger::WriteRecord(const QueuedLogRecord& record) noexcept {
    if (!output_.is_open()) {
        return;
    }
    if (file_bytes_written_ >= configuration_.maximum_file_size_bytes) {
        metrics_.suppressed_by_file_limit.fetch_add(1U, std::memory_order_relaxed);
        return;
    }

    try {
        const std::string_view msg(record.message.data(), record.message_length);
        const std::string line =
            std::format("{} [{}] {}\n",
                        CurrentTimestamp(),
                        ToString(record.severity),
                        msg);

        output_ << line;
        output_.flush();

        if (output_.fail()) {
            metrics_.file_write_failures.fetch_add(1U, std::memory_order_relaxed);
            return;
        }

        file_bytes_written_ += line.size();
        metrics_.written_records.fetch_add(1U, std::memory_order_relaxed);
    } catch (...) {
        metrics_.file_write_failures.fetch_add(1U, std::memory_order_relaxed);
    }
}

AsyncLogger::QueuedLogRecord AsyncLogger::MakeRealtimeEventRecord(
    const RealtimeEvent& event) const noexcept {
    try {
        const std::string text = std::format(
            "[rt-event] type={} source={} code={} ts={} a={} b={}",
            static_cast<std::uint16_t>(event.type),
            event.source_id,
            event.code,
            event.monotonic_timestamp_ticks,
            event.value_a,
            event.value_b);
        return MakeRecord(event.severity, text);
    } catch (...) {
        return MakeRecord(event.severity, "[rt-event] format error");
    }
}

} // namespace adaptive_audio::foundation
