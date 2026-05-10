#include "bot.hpp"
#include "modules/warning_module.hpp"
#include "modules/roll_module.hpp"
#include "modules/message_module.hpp"
#include "modules/tournament_module.hpp"

#include <dpp/dpp.h>

#include <algorithm>
#include <format>

namespace bot {

Bot::Bot(Config config)
    : config_{std::move(config)}
    , store_{config_.database_path}
{
    // Create the DPP cluster
    cluster_ = std::make_unique<dpp::cluster>(
        config_.bot_token,
        dpp::i_default_intents | dpp::i_message_content,
        1  // shard count
    );

    // Initialize modules
    warning_module_   = std::make_unique<WarningModule>(*this);
    roll_module_      = std::make_unique<RollModule>(*this);
    message_module_   = std::make_unique<MessageModule>(*this);
    tournament_module_ = std::make_unique<TournamentModule>(*this);

    setup_handlers();
    register_commands();
}

void Bot::run() {
    // Load persistent data
    auto load_result = store_.load();
    if (!load_result.has_value()) {
        cluster_->log(dpp::ll_warning,
            std::format("Failed to load store: {}", load_result.error()));
    }

    // Set up periodic save every 60 seconds
    save_timer_ = cluster_->start_timer([this](dpp::timer) {
        auto result = save_state();
        if (!result.has_value()) {
            cluster_->log(dpp::ll_warning,
                std::format("Periodic save failed: {}", result.error()));
        }
    }, 60);

    cluster_->log(dpp::ll_info, "Bot starting...");
    cluster_->start(dpp::st_wait);
}

auto Bot::is_admin(dpp::snowflake guild_id, const dpp::user& user) const -> bool {
    // If no admin roles configured, allow everyone (for initial setup)
    if (config_.admin_roles.empty()) {
        return true;
    }

    // Look up the member in the guild
    auto* guild = dpp::find_guild(guild_id);
    if (!guild) return false;

    auto member_it = guild->members.find(user.id);
    if (member_it == guild->members.end()) return false;

    const auto& member_roles = member_it->second.get_roles();

    // Check if any of the user's roles match admin roles
    return std::ranges::any_of(config_.admin_roles,
        [&member_roles](dpp::snowflake admin_role) {
            return std::ranges::find(member_roles, admin_role)
                   != member_roles.end();
        });
}

auto Bot::save_state() -> std::expected<void, std::string> {
    auto store_result = store_.save();
    if (!store_result.has_value()) {
        return store_result;
    }

    // Also save config (warning roles may have been updated)
    std::filesystem::path cfg_path = "config.json";
    return config_.save(cfg_path);
}

void Bot::setup_handlers() {
    // Log events
    cluster_->on_log([this](const dpp::log_t& event) {
        // DPP already logs to console, we just pass through
    });

    // Ready event — bot is connected
    cluster_->on_ready([this](const dpp::ready_t& event) {
        cluster_->log(dpp::ll_info, "Bot is connected and ready!");

        // Register global slash commands
        if (dpp::run_once<struct register_commands_tag>()) {
            register_commands();
        }
    });

    // Slash command interactions
    cluster_->on_slashcommand([this](const dpp::slashcommand_t& event) {
        const auto& cmd_name = event.command.get_command_name();

        // Route to appropriate module
        if (cmd_name == "warning") {
            warning_module_->handle_command(event);
        } else if (cmd_name == "roll") {
            roll_module_->handle_command(event);
        } else if (cmd_name == "message") {
            message_module_->handle_command(event);
        } else if (cmd_name == "createlobby" || cmd_name == "lobbylist"
                   || cmd_name == "delete" || cmd_name == "setwinner"
                   || cmd_name == "startmatch") {
            tournament_module_->handle_command(event);
        } else {
            event.reply("Unknown command.");
        }
    });

    // Button click interactions
    cluster_->on_button_click([this](const dpp::button_click_t& event) {
        // Parse custom_id to route to module
        const auto& custom_id = event.custom_id;

        if (custom_id.starts_with("roll_participate_")) {
            roll_module_->handle_button(event);
        } else if (custom_id.starts_with("bracket_refresh_")) {
            tournament_module_->handle_button(event);
        } else if (custom_id.starts_with("lobby_join_")) {
            tournament_module_->handle_button(event);
        }
    });
}

void Bot::register_commands() {
    // ── /warning ──────────────────────────────────────────────────────────

    auto warning_cmd = dpp::slashcommand("warning", "Manage user warnings", cluster_->me.id)
        .add_option(dpp::command_option(dpp::co_sub_command, "add",
            "Issue a warning to a user")
            .add_option(dpp::command_option(dpp::co_user, "user",
                "The user to warn", true))
            .add_option(dpp::command_option(dpp::co_string, "reason",
                "Reason for the warning", false))
        )
        .add_option(dpp::command_option(dpp::co_sub_command, "remove",
            "Remove a warning from a user")
            .add_option(dpp::command_option(dpp::co_user, "user",
                "The user to unwarn", true))
        )
        .add_option(dpp::command_option(dpp::co_sub_command, "set",
            "Set roles to assign on warning")
            .add_option(dpp::command_option(dpp::co_string, "roles",
                "Comma-separated role IDs", true))
        )
        .add_option(dpp::command_option(dpp::co_sub_command, "list",
            "List all warned users"));

    // ── /roll ─────────────────────────────────────────────────────────────

    auto roll_cmd = dpp::slashcommand("roll", "Prize raffle system", cluster_->me.id)
        .add_option(dpp::command_option(dpp::co_sub_command, "start",
            "Start a new prize roll")
            .add_option(dpp::command_option(dpp::co_integer, "time",
                "Duration in seconds (30-172800)", true))
            .add_option(dpp::command_option(dpp::co_string, "prize",
                "Prize description", true))
        )
        .add_option(dpp::command_option(dpp::co_sub_command, "emergency",
            "Force an immediate roll"))
        .add_option(dpp::command_option(dpp::co_sub_command, "delete",
            "Delete the current roll"));

    // ── /message ──────────────────────────────────────────────────────────

    auto message_cmd = dpp::slashcommand("message",
        "Send a DM to all users with a specific role", cluster_->me.id)
        .add_option(dpp::command_option(dpp::co_role, "role",
            "Target role", true))
        .add_option(dpp::command_option(dpp::co_string, "text",
            "Message text to send", true));

    // ── /createlobby ──────────────────────────────────────────────────────

    auto createlobby_cmd = dpp::slashcommand("createlobby",
        "Create a tournament lobby", cluster_->me.id)
        .add_option(dpp::command_option(dpp::co_string, "name",
            "Lobby name", true))
        .add_option(dpp::command_option(dpp::co_integer, "team_size",
            "Players per team (1=solo, 2=duo, 3=trio)", true))
        .add_option(dpp::command_option(dpp::co_string, "format",
            "Tournament format: single_elimination, double_elimination, round_robin",
            false))
        .add_option(dpp::command_option(dpp::co_string, "questionnaire",
            "Criteria/questions for participants", false));

    // ── /lobbylist ────────────────────────────────────────────────────────

    auto lobbylist_cmd = dpp::slashcommand("lobbylist",
        "List all tournament lobbies with bracket", cluster_->me.id)
        .add_option(dpp::command_option(dpp::co_string, "name",
            "Specific lobby name to show", false));

    // ── /delete ───────────────────────────────────────────────────────────

    auto delete_cmd = dpp::slashcommand("delete",
        "Delete a team from a tournament", cluster_->me.id)
        .add_option(dpp::command_option(dpp::co_string, "lobby",
            "Lobby name", true))
        .add_option(dpp::command_option(dpp::co_string, "team",
            "Team name to delete", true));

    // ── /setwinner ────────────────────────────────────────────────────────

    auto setwinner_cmd = dpp::slashcommand("setwinner",
        "Set the winner of a match", cluster_->me.id)
        .add_option(dpp::command_option(dpp::co_string, "lobby",
            "Lobby name", true))
        .add_option(dpp::command_option(dpp::co_string, "match_id",
            "Match ID", true))
        .add_option(dpp::command_option(dpp::co_string, "winner",
            "Winning team name", true));

    // ── /startmatch ───────────────────────────────────────────────────────

    auto startmatch_cmd = dpp::slashcommand("startmatch",
        "Start a match", cluster_->me.id)
        .add_option(dpp::command_option(dpp::co_string, "lobby",
            "Lobby name", true))
        .add_option(dpp::command_option(dpp::co_string, "match_id",
            "Match ID", true));

    // Register all commands globally
    cluster_->global_bulk_command_create({
        warning_cmd,
        roll_cmd,
        message_cmd,
        createlobby_cmd,
        lobbylist_cmd,
        delete_cmd,
        setwinner_cmd,
        startmatch_cmd,
    });
}

} // namespace bot
