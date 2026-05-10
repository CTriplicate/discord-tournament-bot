#include "modules/tournament_module.hpp"
#include "bot.hpp"
#include "bracket/bracket_renderer.hpp"

#include <dpp/dpp.h>
#include <algorithm>
#include <cmath>
#include <format>
#include <random>

namespace bot {

TournamentModule::TournamentModule(Bot& bot)
    : bot_(bot) {}

void TournamentModule::handle_command(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;
    const auto& cmd_name = event.command.get_command_name();

    // All tournament commands require admin
    if (!bot_.is_admin(gid, event.command.get_issuing_user())) {
        event.reply(dpp::message("You don't have permission to use this command.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    if (cmd_name == "createlobby") {
        cmd_create_lobby(event);
    } else if (cmd_name == "lobbylist") {
        cmd_lobby_list(event);
    } else if (cmd_name == "delete") {
        cmd_delete_team(event);
    } else if (cmd_name == "setwinner") {
        cmd_set_winner(event);
    } else if (cmd_name == "startmatch") {
        cmd_start_match(event);
    } else {
        event.reply("Unknown tournament command.");
    }
}

void TournamentModule::handle_button(const dpp::button_click_t& event) {
    const auto& custom_id = event.custom_id;

    if (custom_id.starts_with("bracket_refresh_")) {
        // Format: bracket_refresh_<guild_id>_<lobby_name>
        // Extract guild_id and lobby_name
        auto parts = custom_id.substr(std::string("bracket_refresh_").size());
        auto underscore_pos = parts.find('_');
        if (underscore_pos == std::string::npos) {
            event.reply(dpp::message("Invalid button data.").set_flags(dpp::m_ephemeral));
            return;
        }

        auto guild_str = parts.substr(0, underscore_pos);
        auto lobby_name = parts.substr(underscore_pos + 1);

        dpp::snowflake guild_id{std::stoull(guild_str)};

        auto lobby = bot_.store().get_lobby(guild_id, lobby_name);
        if (!lobby.has_value()) {
            event.reply(dpp::message("Lobby no longer exists.").set_flags(dpp::m_ephemeral));
            return;
        }

        post_bracket(event.command.channel_id, *lobby);
        event.reply(dpp::message("Bracket refreshed!").set_flags(dpp::m_ephemeral));
    } else if (custom_id.starts_with("lobby_join_")) {
        // Future: join a team via button
        event.reply(dpp::message("Team join functionality — use /createlobby to set up teams.")
            .set_flags(dpp::m_ephemeral));
    }
}

void TournamentModule::cmd_create_lobby(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;
    const auto cid = event.command.channel_id;

    std::string lobby_name;
    int64_t team_size = 1;
    std::string format_str = "single_elimination";
    std::string questionnaire;

    for (const auto& opt : event.command.get_options()) {
        if (opt.name == "name" && opt.type == dpp::co_string) {
            lobby_name = std::get<std::string>(opt.value);
        } else if (opt.name == "team_size" && opt.type == dpp::co_integer) {
            team_size = std::get<int64_t>(opt.value);
        } else if (opt.name == "format" && opt.type == dpp::co_string) {
            format_str = std::get<std::string>(opt.value);
        } else if (opt.name == "questionnaire" && opt.type == dpp::co_string) {
            questionnaire = std::get<std::string>(opt.value);
        }
    }

    if (lobby_name.empty()) {
        event.reply(dpp::message("Lobby name is required.").set_flags(dpp::m_ephemeral));
        return;
    }

    if (team_size < 1 || team_size > 10) {
        event.reply(dpp::message("Team size must be between 1 and 10.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Parse tournament format
    TournamentFormat format = TournamentFormat::SingleElimination;
    if (format_str == "double_elimination") {
        format = TournamentFormat::DoubleElimination;
    } else if (format_str == "round_robin") {
        format = TournamentFormat::RoundRobin;
    }

    Lobby lobby{
        .guild_id       = gid,
        .channel_id     = cid,
        .creator_id     = event.command.get_issuing_user().id,
        .lobby_name     = lobby_name,
        .team_size      = static_cast<int>(team_size),
        .format         = format,
        .questionnaire  = questionnaire,
    };

    auto result = bot_.store().create_lobby(lobby);
    if (!result.has_value()) {
        event.reply(dpp::message(result.error()).set_flags(dpp::m_ephemeral));
        return;
    }

    bot_.save_state();

    // Build response
    std::string format_display;
    switch (format) {
        case TournamentFormat::SingleElimination: format_display = "Single Elimination"; break;
        case TournamentFormat::DoubleElimination: format_display = "Double Elimination"; break;
        case TournamentFormat::RoundRobin:        format_display = "Round Robin"; break;
    }

    auto embed = dpp::embed()
        .set_color(0x9B59B6)
        .set_title(std::format("Lobby Created: {}", lobby_name))
        .add_field("Team Size", std::format("{}v{}", team_size, team_size))
        .add_field("Format", format_display)
        .add_field("Questionnaire", questionnaire.empty() ? "None" : questionnaire);

    if (!questionnaire.empty()) {
        embed.add_field("Instructions",
            "Fill out the questionnaire and register your team with an admin.");
    }

    event.reply(embed);
}

void TournamentModule::cmd_lobby_list(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;
    const auto cid = event.command.channel_id;

    // Check if a specific lobby was requested
    std::string specific_name;
    for (const auto& opt : event.command.get_options()) {
        if (opt.name == "name" && opt.type == dpp::co_string) {
            specific_name = std::get<std::string>(opt.value);
        }
    }

    if (!specific_name.empty()) {
        // Show specific lobby with bracket
        auto lobby = bot_.store().get_lobby(gid, specific_name);
        if (!lobby.has_value()) {
            event.reply(dpp::message(
                std::format("Lobby '{}' not found.", specific_name))
                .set_flags(dpp::m_ephemeral));
            return;
        }

        post_bracket(cid, *lobby);
        event.reply(dpp::message(
            std::format("Showing bracket for **{}**", specific_name))
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Show all lobbies overview
    auto lobbies = bot_.store().list_lobbies(gid);

    if (lobbies.empty()) {
        event.reply(dpp::embed()
            .set_color(0x999999)
            .set_title("Tournament Lobbies")
            .set_description("No active lobbies in this guild."));
        return;
    }

    auto embed = dpp::embed()
        .set_color(0x9B59B6)
        .set_title("Tournament Lobbies");

    for (const auto& lobby : lobbies) {
        std::string format_str;
        switch (lobby.format) {
            case TournamentFormat::SingleElimination: format_str = "SE"; break;
            case TournamentFormat::DoubleElimination: format_str = "DE"; break;
            case TournamentFormat::RoundRobin:        format_str = "RR"; break;
        }

        std::string status = lobby.is_bracket_generated ? "Brackets Ready" : "Registration Open";

        embed.add_field(
            std::format("{} ({}v{}, {})", lobby.lobby_name,
                       lobby.team_size, lobby.team_size, format_str),
            std::format("Teams: {} | Status: {}", lobby.teams.size(), status),
            false
        );
    }

    event.reply(embed);
}

void TournamentModule::cmd_delete_team(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;

    std::string lobby_name;
    std::string team_name;

    for (const auto& opt : event.command.get_options()) {
        if (opt.name == "lobby" && opt.type == dpp::co_string) {
            lobby_name = std::get<std::string>(opt.value);
        } else if (opt.name == "team" && opt.type == dpp::co_string) {
            team_name = std::get<std::string>(opt.value);
        }
    }

    auto result = bot_.store().delete_team(gid, lobby_name, team_name);
    if (!result.has_value()) {
        event.reply(dpp::message(result.error()).set_flags(dpp::m_ephemeral));
        return;
    }

    bot_.save_state();

    auto embed = dpp::embed()
        .set_color(0xE74C3C)
        .set_title("Team Deleted")
        .add_field("Lobby", lobby_name)
        .add_field("Team", team_name);

    event.reply(embed);
}

void TournamentModule::cmd_set_winner(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;
    const auto cid = event.command.channel_id;

    std::string lobby_name;
    std::string match_id;
    std::string winner_name;

    for (const auto& opt : event.command.get_options()) {
        if (opt.name == "lobby" && opt.type == dpp::co_string) {
            lobby_name = std::get<std::string>(opt.value);
        } else if (opt.name == "match_id" && opt.type == dpp::co_string) {
            match_id = std::get<std::string>(opt.value);
        } else if (opt.name == "winner" && opt.type == dpp::co_string) {
            winner_name = std::get<std::string>(opt.value);
        }
    }

    auto lobby = bot_.store().get_lobby(gid, lobby_name);
    if (!lobby.has_value()) {
        event.reply(dpp::message(
            std::format("Lobby '{}' not found.", lobby_name))
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Find the match
    auto match_it = std::ranges::find_if(lobby->matches,
        [&match_id](const Match& m) { return m.match_id == match_id; });

    if (match_it == lobby->matches.end()) {
        event.reply(dpp::message(
            std::format("Match '{}' not found.", match_id))
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Validate winner is one of the teams in this match
    if (match_it->team1_name != winner_name && match_it->team2_name != winner_name) {
        event.reply(dpp::message(
            std::format("Team '{}' is not in this match.", winner_name))
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Update the match
    match_it->winner_name = winner_name;
    match_it->status = MatchStatus::Completed;

    // Mark the loser as eliminated
    const auto& loser = (winner_name == match_it->team1_name)
                            ? match_it->team2_name
                            : match_it->team1_name;
    if (loser.has_value()) {
        for (auto& team : lobby->teams) {
            if (team.name == *loser) {
                team.is_eliminated = true;
                break;
            }
        }
    }

    // Advance winner to next round if there is one
    int next_round = match_it->round + 1;
    int next_position = match_it->position / 2;

    auto next_match = std::ranges::find_if(lobby->matches,
        [next_round, next_position](const Match& m) {
            return m.round == next_round && m.position == next_position;
        });

    if (next_match != lobby->matches.end()) {
        // Fill the next match's slot (team1 first, then team2)
        if (!next_match->team1_name.has_value()) {
            next_match->team1_name = winner_name;
        } else {
            next_match->team2_name = winner_name;
        }
    }

    auto update_result = bot_.store().update_lobby(gid, *lobby);
    if (!update_result.has_value()) {
        event.reply(dpp::message(update_result.error()).set_flags(dpp::m_ephemeral));
        return;
    }

    bot_.save_state();

    // Check if tournament is complete (final match has a winner)
    bool is_champion = false;
    if (next_match == lobby->matches.end()) {
        // This was the final
        is_champion = true;
    }

    if (is_champion) {
        auto embed = dpp::embed()
            .set_color(0xFFD700)
            .set_title(std::format("🏆 Tournament Champion: {}!", winner_name))
            .add_field("Lobby", lobby_name);

        bot_.cluster().message_create(dpp::message(cid, embed));
    }

    auto embed = dpp::embed()
        .set_color(0x00CC00)
        .set_title("Winner Set")
        .add_field("Match", match_id)
        .add_field("Winner", winner_name);

    event.reply(embed);

    // Update bracket image
    auto updated_lobby = bot_.store().get_lobby(gid, lobby_name);
    if (updated_lobby.has_value()) {
        post_bracket(cid, *updated_lobby);
    }
}

void TournamentModule::cmd_start_match(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;

    std::string lobby_name;
    std::string match_id;

    for (const auto& opt : event.command.get_options()) {
        if (opt.name == "lobby" && opt.type == dpp::co_string) {
            lobby_name = std::get<std::string>(opt.value);
        } else if (opt.name == "match_id" && opt.type == dpp::co_string) {
            match_id = std::get<std::string>(opt.value);
        }
    }

    auto lobby = bot_.store().get_lobby(gid, lobby_name);
    if (!lobby.has_value()) {
        event.reply(dpp::message(
            std::format("Lobby '{}' not found.", lobby_name))
            .set_flags(dpp::m_ephemeral));
        return;
    }

    auto match_it = std::ranges::find_if(lobby->matches,
        [&match_id](const Match& m) { return m.match_id == match_id; });

    if (match_it == lobby->matches.end()) {
        event.reply(dpp::message(
            std::format("Match '{}' not found.", match_id))
            .set_flags(dpp::m_ephemeral));
        return;
    }

    if (match_it->status != MatchStatus::Pending) {
        event.reply(dpp::message(
            std::format("Match '{}' is already in progress or completed.", match_id))
            .set_flags(dpp::m_ephemeral));
        return;
    }

    if (!match_it->team1_name.has_value() || !match_it->team2_name.has_value()) {
        event.reply(dpp::message(
            "Both teams must be determined before starting a match.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    match_it->status = MatchStatus::InProgress;

    auto update_result = bot_.store().update_lobby(gid, *lobby);
    if (!update_result.has_value()) {
        event.reply(dpp::message(update_result.error()).set_flags(dpp::m_ephemeral));
        return;
    }

    bot_.save_state();

    auto embed = dpp::embed()
        .set_color(0x3498DB)
        .set_title(std::format("Match Started: {}", match_id))
        .add_field("Team 1", *match_it->team1_name, true)
        .add_field("vs", "⚔️", true)
        .add_field("Team 2", *match_it->team2_name, true)
        .set_footer(dpp::embed_footer()
            .set_text("Use /setwinner to set the result"));

    event.reply(embed);
}

auto TournamentModule::generate_bracket(Lobby& lobby) -> std::expected<void, std::string> {
    if (lobby.teams.size() < 2) {
        return std::unexpected("Need at least 2 teams to generate a bracket");
    }

    lobby.matches.clear();

    // Calculate the next power of 2 for the bracket size
    size_t num_teams = lobby.teams.size();
    size_t bracket_size = 1;
    while (bracket_size < num_teams) {
        bracket_size *= 2;
    }

    // Number of byes in the first round
    size_t byes = bracket_size - num_teams;

    // Shuffle teams for random seeding
    std::random_device rd;
    std::mt19937 gen(rd());
    std::vector<size_t> indices(num_teams);
    std::iota(indices.begin(), indices.end(), 0);
    std::ranges::shuffle(indices, gen);

    // Calculate total rounds
    int total_rounds = static_cast<int>(std::log2(bracket_size));

    // Create all matches for all rounds
    for (int round = 0; round < total_rounds; ++round) {
        int matches_in_round = static_cast<int>(bracket_size) / (1 << (round + 1));
        for (int pos = 0; pos < matches_in_round; ++pos) {
            Match m{
                .match_id  = std::format("R{}M{}", round + 1, pos + 1),
                .round     = round,
                .position  = pos,
                .status    = MatchStatus::Pending,
            };
            lobby.matches.push_back(m);
        }
    }

    // Fill first round with teams
    size_t team_idx = 0;
    for (int pos = 0; pos < static_cast<int>(bracket_size / 2); ++pos) {
        auto match_it = std::ranges::find_if(lobby.matches,
            [pos](const Match& m) { return m.round == 0 && m.position == pos; });

        if (match_it == lobby.matches.end()) break;

        // Team 1
        if (team_idx < indices.size()) {
            match_it->team1_name = lobby.teams[indices[team_idx]].name;
            team_idx++;
        }

        // Team 2 — if we have a bye, the team advances automatically
        if (team_idx < indices.size()) {
            match_it->team2_name = lobby.teams[indices[team_idx]].name;
            team_idx++;
        } else if (match_it->team1_name.has_value()) {
            // Bye: team1 advances to next round automatically
            int next_pos = pos / 2;
            auto next_match = std::ranges::find_if(lobby.matches,
                [next_pos](const Match& m) { return m.round == 1 && m.position == next_pos; });
            if (next_match != lobby.matches.end()) {
                if (!next_match->team1_name.has_value()) {
                    next_match->team1_name = match_it->team1_name;
                } else {
                    next_match->team2_name = match_it->team1_name;
                }
            }
            match_it->winner_name = match_it->team1_name;
            match_it->status = MatchStatus::Completed;
        }
    }

    lobby.is_bracket_generated = true;
    return {};
}

void TournamentModule::post_bracket(dpp::snowflake channel_id, const Lobby& lobby) {
    // If bracket not generated yet, try to generate it
    if (!lobby.is_bracket_generated && lobby.teams.size() >= 2) {
        // Need a non-const copy to generate
        Lobby mutable_lobby = lobby;
        auto result = generate_bracket(mutable_lobby);
        if (result.has_value()) {
            bot_.store().update_lobby(lobby.guild_id, mutable_lobby);
            bot_.save_state();
            post_bracket(channel_id, mutable_lobby);
            return;
        }
    }

    // Render bracket image
    BracketRenderer renderer;
    auto img_result = renderer.render(lobby);

    if (img_result.has_value()) {
        auto& image_path = *img_result;

        // Upload image to Discord
        bot_.cluster().message_create(
            dpp::message(channel_id, "")
                .add_file("bracket.png", dpp::utility::read_file(image_path))
                .add_component(
                    dpp::component()
                        .add_component(
                            dpp::component()
                                .set_type(dpp::cot_button)
                                .set_label("Refresh")
                                .set_style(dpp::cos_primary)
                                .set_emoji("🔄")
                                .set_custom_id(std::format("bracket_refresh_{}_{}",
                                    static_cast<uint64_t>(lobby.guild_id),
                                    lobby.lobby_name))
                        )
                ),
            [this](const dpp::confirmation_callback_t& cb) {
                if (cb.is_error()) {
                    bot_.cluster().log(dpp::ll_warning,
                        std::format("Failed to post bracket: {}",
                                   cb.get_error().message));
                }
            }
        );
    } else {
        // Fallback: text-based bracket
        std::string bracket_text = "**Tournament Bracket: " + lobby.lobby_name + "**\n```\n";

        // Group matches by round
        auto max_round = 0;
        for (const auto& m : lobby.matches) {
            max_round = std::max(max_round, m.round);
        }

        for (int round = 0; round <= max_round; ++round) {
            bracket_text += std::format("── Round {} ──\n", round + 1);
            for (const auto& m : lobby.matches) {
                if (m.round != round) continue;

                auto t1 = m.team1_name.value_or("TBD");
                auto t2 = m.team2_name.value_or("TBD");
                auto winner = m.winner_name.value_or("-");

                std::string status_icon;
                switch (m.status) {
                    case MatchStatus::Pending:    status_icon = "⏳"; break;
                    case MatchStatus::InProgress: status_icon = "⚔️"; break;
                    case MatchStatus::Completed:  status_icon = "✅"; break;
                }

                bracket_text += std::format("  {} {} vs {} → {} {}\n",
                    status_icon, t1, t2, winner, m.match_id);
            }
        }
        bracket_text += "```";

        bot_.cluster().message_create(
            dpp::message(channel_id, bracket_text)
                .add_component(
                    dpp::component()
                        .add_component(
                            dpp::component()
                                .set_type(dpp::cot_button)
                                .set_label("Refresh")
                                .set_style(dpp::cos_primary)
                                .set_emoji("🔄")
                                .set_custom_id(std::format("bracket_refresh_{}_{}",
                                    static_cast<uint64_t>(lobby.guild_id),
                                    lobby.lobby_name))
                        )
                )
        );
    }
}

} // namespace bot
