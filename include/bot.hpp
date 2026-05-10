#pragma once

#include <memory>
#include <string>

#include <dpp/dpp.h>

#include "config.hpp"
#include "store.hpp"

namespace bot {

// Forward declarations for modules
class WarningModule;
class RollModule;
class MessageModule;
class TournamentModule;

/// Core bot class. Owns the DPP cluster, the data store, and all modules.
/// Responsible for registering slash commands and dispatching interactions.
class Bot {
public:
    /// Construct and configure the bot.
    /// @param config  Loaded bot configuration
    explicit Bot(Config config);

    /// Non-copyable, non-movable (owns the DPP cluster).
    Bot(const Bot&) = delete;
    Bot& operator=(const Bot&) = delete;
    Bot(Bot&&) = delete;
    Bot& operator=(Bot&&) = delete;

    ~Bot() = default;

    /// Start the bot (blocking).
    void run();

    /// Accessors for modules
    [[nodiscard]] auto cluster() -> dpp::cluster& { return *cluster_; }
    [[nodiscard]] auto store() -> Store& { return store_; }
    [[nodiscard]] auto config() -> Config& { return config_; }
    [[nodiscard]] auto config() const -> const Config& { return config_; }

    /// Check if a user has admin privileges (has one of the configured admin roles).
    [[nodiscard]] auto is_admin(dpp::snowflake guild_id,
                                 const dpp::user& user) const -> bool;

    /// Persist current state to disk.
    auto save_state() -> std::expected<void, std::string>;

private:
    /// Register all slash commands with Discord.
    void register_commands();

    /// Set up event handlers.
    void setup_handlers();

    Config config_;
    Store store_;

    // DPP cluster — heap-allocated because it's heavy and non-movable.
    std::unique_ptr<dpp::cluster> cluster_;

    // Modules — heap-allocated for stable pointers and to avoid circular deps.
    std::unique_ptr<WarningModule>   warning_module_;
    std::unique_ptr<RollModule>      roll_module_;
    std::unique_ptr<MessageModule>   message_module_;
    std::unique_ptr<TournamentModule> tournament_module_;

    // Timer handle for periodic saves
    dpp::timer save_timer_{};
};

} // namespace bot
