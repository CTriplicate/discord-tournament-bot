#pragma once

#include <dpp/dpp.h>
#include <string>

namespace bot {

class Bot;

/// Handles tournament/lobby slash commands and button interactions.
///
/// Commands:
///   /createlobby name team_size [format] [questionnaire]
///   /lobbylist [name]
///   /delete lobby team
///   /setwinner lobby match_id winner
///   /startmatch lobby match_id
///
/// Buttons:
///   "bracket_refresh_<guild_id>_<lobby_name>" — refresh bracket image
///   "lobby_join_<guild_id>_<lobby_name>"       — join a lobby/team
class TournamentModule {
public:
    explicit TournamentModule(Bot& bot);

    /// Dispatch tournament-related slash commands.
    void handle_command(const dpp::slashcommand_t& event);

    /// Handle tournament-related button clicks.
    void handle_button(const dpp::button_click_t& event);

private:
    /// /createlobby name team_size [format] [questionnaire]
    void cmd_create_lobby(const dpp::slashcommand_t& event);

    /// /lobbylist [name]
    void cmd_lobby_list(const dpp::slashcommand_t& event);

    /// /delete lobby team
    void cmd_delete_team(const dpp::slashcommand_t& event);

    /// /setwinner lobby match_id winner
    void cmd_set_winner(const dpp::slashcommand_t& event);

    /// /startmatch lobby match_id
    void cmd_start_match(const dpp::slashcommand_t& event);

    /// Generate a single-elimination bracket from the current teams.
    /// Populates the matches vector in the lobby.
    auto generate_bracket(Lobby& lobby) -> std::expected<void, std::string>;

    /// Post/update the bracket image in the channel.
    void post_bracket(dpp::snowflake channel_id, const Lobby& lobby);

    Bot& bot_;
};

} // namespace bot
