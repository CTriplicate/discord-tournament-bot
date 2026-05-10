#pragma once

#include <dpp/dpp.h>

namespace bot {

class Bot;

/// Handles the /warning family of slash commands.
///
/// Commands:
///   /warning add   @user [reason]  — issue a warning + assign configured roles
///   /warning remove @user           — remove one warning + remove roles if count reaches 0
///   /warning set   roles           — set per-guild warning roles (comma-separated IDs)
///   /warning list                   — list all warned users in the guild
class WarningModule {
public:
    explicit WarningModule(Bot& bot);

    /// Dispatch a /warning slash command based on subcommand.
    void handle_command(const dpp::slashcommand_t& event);

private:
    /// /warning add @user [reason]
    void cmd_add(const dpp::slashcommand_t& event);

    /// /warning remove @user
    void cmd_remove(const dpp::slashcommand_t& event);

    /// /warning set roles
    void cmd_set(const dpp::slashcommand_t& event);

    /// /warning list
    void cmd_list(const dpp::slashcommand_t& event);

    /// Assign warning roles to a user in a guild.
    void assign_warning_roles(dpp::snowflake guild_id, dpp::snowflake user_id);

    /// Remove warning roles from a user in a guild.
    void remove_warning_roles(dpp::snowflake guild_id, dpp::snowflake user_id);

    Bot& bot_;
};

} // namespace bot
