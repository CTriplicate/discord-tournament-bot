#include "bot.hpp"
#include "config.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

/// Read an environment variable, returns empty string if not set.
static auto get_env(const char* name) -> std::string {
    const char* val = std::getenv(name);
    return val ? std::string{val} : std::string{};
}

auto main(int argc, char* argv[]) -> int {
    // Determine config path
    std::filesystem::path config_path = "config.json";
    if (argc > 1) {
        config_path = argv[1];
    }

    // Load configuration from file (if exists)
    auto config_result = bot::Config::load(config_path);

    // If config file doesn't exist or is incomplete, create a default
    // and override with environment variables (Railway-style deployment).
    auto config = config_result.value_or(bot::Config{});

    // Environment variable overrides (take precedence over file)
    auto env_token = get_env("BOT_TOKEN");
    if (!env_token.empty()) {
        config.bot_token = env_token;
    }

    auto env_db_path = get_env("DATABASE_PATH");
    if (!env_db_path.empty()) {
        config.database_path = env_db_path;
    }

    auto env_bracket_dir = get_env("BRACKET_OUTPUT_DIR");
    if (!env_bracket_dir.empty()) {
        config.bracket_output_dir = env_bracket_dir;
    }

    auto env_admin_roles = get_env("ADMIN_ROLES");
    if (!env_admin_roles.empty()) {
        // Comma-separated role IDs
        config.admin_roles.clear();
        std::istringstream ss(env_admin_roles);
        std::string token;
        while (std::getline(ss, token, ',')) {
            auto start = token.find_first_not_of(" \t");
            auto end = token.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                config.admin_roles.emplace_back(
                    std::stoull(token.substr(start, end - start + 1)));
            }
        }
    }

    if (config.bot_token.empty()) {
        std::cerr << "Error: BOT_TOKEN is not set. Provide it via:\n"
                  << "  1. config.json (\"bot_token\" field)\n"
                  << "  2. Environment variable BOT_TOKEN\n";
        return EXIT_FAILURE;
    }

    std::cout << "Configuration loaded successfully.\n";
    std::cout << "  Database: " << config.database_path << "\n";
    std::cout << "  Bracket output: " << config.bracket_output_dir << "\n";
    std::cout << "  Admin roles: " << config.admin_roles.size() << "\n";
    std::cout << "  Warning configs: " << config.warning_configs.size() << " guild(s)\n";

    // Ensure data directories exist
    if (!std::filesystem::exists(config.database_path)) {
        auto parent = std::filesystem::path(config.database_path).parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
    }
    if (!std::filesystem::exists(config.bracket_output_dir)) {
        std::filesystem::create_directories(config.bracket_output_dir);
    }

    // Create and run the bot
    try {
        bot::Bot bot(std::move(config));
        bot.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
