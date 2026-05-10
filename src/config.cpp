#include "config.hpp"

#include <fstream>

namespace bot {

// ── JSON serialization for WarningConfig ─────────────────────────────────────

void to_json(nlohmann::json& j, const WarningConfig& wc) {
    auto roles = nlohmann::json::array();
    for (const auto& role : wc.warning_roles) {
        roles.push_back(std::to_string(static_cast<uint64_t>(role)));
    }
    j = nlohmann::json{{"warning_roles", roles}};
}

void from_json(const nlohmann::json& j, WarningConfig& wc) {
    wc.warning_roles.clear();
    if (j.contains("warning_roles") && j["warning_roles"].is_array()) {
        for (const auto& r : j["warning_roles"]) {
            wc.warning_roles.emplace_back(std::stoull(r.get<std::string>()));
        }
    }
}

// ── JSON serialization for Config ────────────────────────────────────────────

void to_json(nlohmann::json& j, const Config& c) {
    auto admin = nlohmann::json::array();
    for (const auto& r : c.admin_roles) {
        admin.push_back(std::to_string(static_cast<uint64_t>(r)));
    }

    auto warn_cfgs = nlohmann::json::object();
    for (const auto& [gid, wc] : c.warning_configs) {
        warn_cfgs[std::to_string(static_cast<uint64_t>(gid))] = wc;
    }

    j = nlohmann::json{
        {"bot_token",         c.bot_token},
        {"database_path",     c.database_path},
        {"bracket_output_dir", c.bracket_output_dir},
        {"admin_roles",       admin},
        {"warning_configs",   warn_cfgs},
    };
}

void from_json(const nlohmann::json& j, Config& c) {
    c.bot_token = j.value("bot_token", "");
    c.database_path = j.value("database_path", "data/bot_data.json");
    c.bracket_output_dir = j.value("bracket_output_dir", "data/brackets");

    c.admin_roles.clear();
    if (j.contains("admin_roles") && j["admin_roles"].is_array()) {
        for (const auto& r : j["admin_roles"]) {
            c.admin_roles.emplace_back(std::stoull(r.get<std::string>()));
        }
    }

    c.warning_configs.clear();
    if (j.contains("warning_configs") && j["warning_configs"].is_object()) {
        for (const auto& [gid_str, wc_json] : j["warning_configs"].items()) {
            dpp::snowflake gid{std::stoull(gid_str)};
            c.warning_configs[gid] = wc_json.get<WarningConfig>();
        }
    }
}

// ── Load / Save ──────────────────────────────────────────────────────────────

auto Config::load(const std::filesystem::path& path) -> std::expected<Config, std::string> {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(
            std::format("Config file not found: {}", path.string()));
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return std::unexpected(
            std::format("Cannot open config file: {}", path.string()));
    }

    try {
        auto j = nlohmann::json::parse(ifs);
        auto cfg = j.get<Config>();

        if (cfg.bot_token.empty()) {
            return std::unexpected("bot_token is empty in config");
        }

        return cfg;
    } catch (const nlohmann::json::exception& e) {
        return std::unexpected(
            std::format("JSON parse error in config: {}", e.what()));
    }
}

auto Config::save(const std::filesystem::path& path) const -> std::expected<void, std::string> {
    try {
        auto parent = path.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }

        std::ofstream ofs(path);
        if (!ofs.is_open()) {
            return std::unexpected(
                std::format("Cannot open config for writing: {}", path.string()));
        }

        nlohmann::json j = *this;
        ofs << j.dump(4);
        return {};
    } catch (const std::exception& e) {
        return std::unexpected(
            std::format("Failed to save config: {}", e.what()));
    }
}

} // namespace bot
