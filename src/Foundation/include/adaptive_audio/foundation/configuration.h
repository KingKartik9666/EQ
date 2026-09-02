#pragma once

#include "adaptive_audio/foundation/log_severity.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace adaptive_audio::foundation {

inline constexpr std::uint32_t kConfigurationSchemaVersion{1U};
inline constexpr std::size_t kMaximumConfigurationFileBytes{64U * 1024U};

// Configuration is owned and changed only by control-plane code. No audio
// callback may call the parser, file loader, validator, or serializer.
struct ApplicationConfiguration final {
    std::uint32_t schema_version{kConfigurationSchemaVersion};
    bool start_minimized{false};
    bool telemetry_enabled{false};
    std::string preferred_render_endpoint_id{};
    std::string log_directory{"logs"};
    LogSeverity minimum_log_severity{LogSeverity::Info};
};

struct ConfigurationLoadResult final {
    std::optional<ApplicationConfiguration> configuration{};
    std::string error{};
    std::size_t error_line{0U};

    [[nodiscard]] explicit operator bool() const noexcept {
        return configuration.has_value();
    }
};

// All functions below are explicitly non-real-time. LoadConfigurationFromFile
// performs filesystem I/O; ParseConfiguration and SerializeConfiguration may
// allocate. The file format is a strict, versioned key=value document.
[[nodiscard]] ConfigurationLoadResult ParseConfiguration(std::string_view text);
[[nodiscard]] ConfigurationLoadResult LoadConfigurationFromFile(
    const std::filesystem::path& path);
[[nodiscard]] bool ValidateConfiguration(const ApplicationConfiguration& configuration,
                                         std::string& error);
[[nodiscard]] std::string SerializeConfiguration(const ApplicationConfiguration& configuration);

} // namespace adaptive_audio::foundation
