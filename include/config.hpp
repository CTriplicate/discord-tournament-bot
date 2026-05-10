#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace bot {

/// Per-guild configuration for the warning system.
struct WarningConfig {
    /// Roles to assign when a user receives a warning.
    std::vector<dpp::snowflake> warning_roles{};
};

/// Top-level bot configuration, loaded from a JSON file.
struct Config {
    std::string bot_token{};
    std::string database_path{"data/bot_data.json"};
    std::string bracket_output_dir{"data/brackets"};

    /// Roles that have admin access to bot commands.
    std::vector<dpp::snowflake> admin_roles{};

    /// Per-guild warning configuration.
    std::unordered_map<dpp::snowflake, WarningConfig> warning_configs{};

    /// Loads configuration from a JSON file.
    /// Returns the config on success, or an error string on failure.
    static auto load(const std::filesystem::path& path) -> std::expected<Config, std::string>;

    /// Saves current configuration back to the JSON file.
    [[nodiscard]] auto save(const std::filesystem::path& path) const -> std::expected<void, std::string>;
};

void to_json(nlohmann::json& j, const WarningConfig& wc);
void from_json(const nlohmann::json& j, WarningConfig& wc);

void to_json(nlohmann::json& j, const Config& c);
void from_json(const nlohmann::json& j, Config& c);

} // namespace bot
