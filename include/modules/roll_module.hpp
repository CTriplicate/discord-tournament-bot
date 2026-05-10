#pragma once

#include <dpp/dpp.h>
#include <unordered_map>

namespace bot {

class Bot;

/// Handles the /roll family of slash commands and button interactions.
///
/// Commands:
///   /roll start time:<30-172800> prize:<desc> — start a raffle with a participation button
///   /roll emergency                          — force immediate roll
///   /roll delete                             — delete current roll
///
/// Button:
///   "roll_participate_<channel_id>" — user clicks to join the raffle
class RollModule {
public:
    explicit RollModule(Bot& bot);

    /// Dispatch a /roll slash command.
    void handle_command(const dpp::slashcommand_t& event);

    /// Handle "participate" button click.
    void handle_button(const dpp::button_click_t& event);

private:
    /// /roll start time prize
    void cmd_start(const dpp::slashcommand_t& event);

    /// /roll emergency
    void cmd_emergency(const dpp::slashcommand_t& event);

    /// /roll delete
    void cmd_delete(const dpp::slashcommand_t& event);

    /// Perform the actual random selection and announce the winner.
    void execute_roll(dpp::snowflake channel_id,
                      const RollData& roll_data,
                      dpp::snowflake guild_id);

    /// Schedule a roll to execute after the specified duration.
    void schedule_roll(dpp::snowflake channel_id,
                       std::chrono::seconds duration);

    Bot& bot_;

    /// DPP timer handles for scheduled rolls (channel_id -> timer).
    std::unordered_map<dpp::snowflake, dpp::timer> scheduled_timers_;
};

} // namespace bot
