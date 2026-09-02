#pragma once

#include <cstdint>
#include <string_view>

namespace adaptive_audio::foundation {

// This type is intentionally small and trivially copyable so that it can be
// carried in bounded telemetry packets. Formatting belongs to non-real-time
// code in AsyncLogger.
enum class LogSeverity : std::uint8_t {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};

[[nodiscard]] constexpr std::string_view ToString(const LogSeverity severity) noexcept {
    switch (severity) {
    case LogSeverity::Trace:
        return "trace";
    case LogSeverity::Debug:
        return "debug";
    case LogSeverity::Info:
        return "info";
    case LogSeverity::Warning:
        return "warning";
    case LogSeverity::Error:
        return "error";
    case LogSeverity::Critical:
        return "critical";
    }

    return "unknown";
}

[[nodiscard]] constexpr bool MeetsMinimumSeverity(const LogSeverity severity,
                                                   const LogSeverity minimum) noexcept {
    return static_cast<std::uint8_t>(severity) >= static_cast<std::uint8_t>(minimum);
}

} // namespace adaptive_audio::foundation
