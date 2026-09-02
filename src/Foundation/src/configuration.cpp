#include "adaptive_audio/foundation/configuration.h"

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <sstream>
#include <string>

namespace adaptive_audio::foundation {

namespace {

// Returns a string_view with leading and trailing ASCII whitespace removed.
[[nodiscard]] std::string_view Trim(std::string_view s) noexcept {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1U);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1U);
    }
    return s;
}

[[nodiscard]] bool ParseBool(const std::string_view value, bool& out) noexcept {
    if (value == "true"  || value == "1") { out = true;  return true; }
    if (value == "false" || value == "0") { out = false; return true; }
    return false;
}

[[nodiscard]] bool ParseUint32(const std::string_view value, std::uint32_t& out) noexcept {
    std::uint32_t result{};
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec != std::errc{} || ptr != value.data() + value.size()) {
        return false;
    }
    out = result;
    return true;
}

[[nodiscard]] bool ParseLogSeverity(const std::string_view value,
                                    LogSeverity& out) noexcept {
    if (value == "trace")    { out = LogSeverity::Trace;    return true; }
    if (value == "debug")    { out = LogSeverity::Debug;    return true; }
    if (value == "info")     { out = LogSeverity::Info;     return true; }
    if (value == "warning")  { out = LogSeverity::Warning;  return true; }
    if (value == "error")    { out = LogSeverity::Error;    return true; }
    if (value == "critical") { out = LogSeverity::Critical; return true; }
    return false;
}

} // namespace

ConfigurationLoadResult ParseConfiguration(const std::string_view text) {
    ApplicationConfiguration config{};
    bool schema_seen = false;
    std::size_t line_num = 0U;

    std::string_view remaining = text;
    while (!remaining.empty()) {
        ++line_num;

        // Split on newline.
        const std::size_t nl = remaining.find('\n');
        const std::string_view raw_line = (nl == std::string_view::npos)
                                              ? remaining
                                              : remaining.substr(0U, nl);
        remaining = (nl == std::string_view::npos) ? "" : remaining.substr(nl + 1U);

        const std::string_view line = Trim(raw_line);
        if (line.empty() || line.front() == '#') {
            continue; // blank or comment
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string_view::npos) {
            return {.error = std::format("line {}: missing '='", line_num),
                    .error_line = line_num};
        }

        const std::string_view key   = Trim(line.substr(0U, eq));
        const std::string_view value = Trim(line.substr(eq + 1U));

        if (key == "schema_version") {
            std::uint32_t v{};
            if (!ParseUint32(value, v)) {
                return {.error = std::format("line {}: invalid schema_version", line_num),
                        .error_line = line_num};
            }
            if (v != kConfigurationSchemaVersion) {
                return {.error = std::format("line {}: unsupported schema_version {}", line_num, v),
                        .error_line = line_num};
            }
            config.schema_version = v;
            schema_seen = true;
        } else if (key == "start_minimized") {
            if (!ParseBool(value, config.start_minimized)) {
                return {.error = std::format("line {}: invalid bool for start_minimized", line_num),
                        .error_line = line_num};
            }
        } else if (key == "telemetry_enabled") {
            if (!ParseBool(value, config.telemetry_enabled)) {
                return {.error = std::format("line {}: invalid bool for telemetry_enabled", line_num),
                        .error_line = line_num};
            }
        } else if (key == "preferred_render_endpoint_id") {
            config.preferred_render_endpoint_id = std::string(value);
        } else if (key == "log_directory") {
            config.log_directory = std::string(value);
        } else if (key == "minimum_log_severity") {
            if (!ParseLogSeverity(value, config.minimum_log_severity)) {
                return {.error = std::format("line {}: unknown log severity '{}'", line_num, value),
                        .error_line = line_num};
            }
        } else {
            // Unknown key: reject to catch typos in a versioned format.
            return {.error = std::format("line {}: unknown key '{}'", line_num, key),
                    .error_line = line_num};
        }
    }

    if (!schema_seen) {
        return {.error = "schema_version is required", .error_line = 0U};
    }

    return {.configuration = config};
}

ConfigurationLoadResult LoadConfigurationFromFile(const std::filesystem::path& path) {
    try {
        const auto file_size = std::filesystem::file_size(path);
        if (file_size > kMaximumConfigurationFileBytes) {
            return {.error = "configuration file exceeds size limit"};
        }

        std::ifstream file(path, std::ios::in);
        if (!file.is_open()) {
            return {.error = "cannot open configuration file"};
        }

        std::ostringstream buf;
        buf << file.rdbuf();
        if (file.fail() && !file.eof()) {
            return {.error = "error reading configuration file"};
        }

        return ParseConfiguration(buf.str());
    } catch (const std::filesystem::filesystem_error& e) {
        return {.error = std::string("filesystem error: ") + e.what()};
    } catch (...) {
        return {.error = "unexpected error loading configuration"};
    }
}

bool ValidateConfiguration(const ApplicationConfiguration& configuration,
                           std::string& error) {
    if (configuration.schema_version != kConfigurationSchemaVersion) {
        error = std::format("unsupported schema_version {}",
                            configuration.schema_version);
        return false;
    }
    // log_directory must not be empty (may be relative).
    if (configuration.log_directory.empty()) {
        error = "log_directory must not be empty";
        return false;
    }
    return true;
}

std::string SerializeConfiguration(const ApplicationConfiguration& configuration) {
    return std::format(
        "schema_version={}\n"
        "start_minimized={}\n"
        "telemetry_enabled={}\n"
        "preferred_render_endpoint_id={}\n"
        "log_directory={}\n"
        "minimum_log_severity={}\n",
        configuration.schema_version,
        configuration.start_minimized ? "true" : "false",
        configuration.telemetry_enabled ? "true" : "false",
        configuration.preferred_render_endpoint_id,
        configuration.log_directory,
        ToString(configuration.minimum_log_severity));
}

} // namespace adaptive_audio::foundation
