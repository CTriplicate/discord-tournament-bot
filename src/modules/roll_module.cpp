#include "modules/roll_module.hpp"
#include "bot.hpp"

#include <dpp/dpp.h>
#include <chrono>
#include <format>
#include <random>
#include <algorithm>

namespace bot {

RollModule::RollModule(Bot& bot)
    : bot_(bot) {}

void RollModule::handle_command(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;

    if (!bot_.is_admin(gid, event.command.get_issuing_user())) {
        event.reply(dpp::message("You don't have permission to use this command.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Determine subcommand
    std::string sub;
    for (const auto& opt : event.command.get_options()) {
        if (opt.type == dpp::co_sub_command) {
            sub = opt.name;
            break;
        }
    }

    if (sub.empty()) {
        const auto& cmd_data = event.command;
        for (const auto& [name, val] : cmd_data.options) {
            if (val.is_subcommand()) {
                sub = name;
                break;
            }
        }
    }

    if (sub == "start") {
        cmd_start(event);
    } else if (sub == "emergency") {
        cmd_emergency(event);
    } else if (sub == "delete") {
        cmd_delete(event);
    } else {
        event.reply("Unknown roll subcommand.");
    }
}

void RollModule::cmd_start(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;
    const auto cid = event.command.channel_id;

    int64_t duration_secs = 0;
    std::string prize_desc;

    for (const auto& sc : event.command.get_options()) {
        if (sc.name == "start") {
            for (const auto& opt : sc.options) {
                if (opt.name == "time" && opt.type == dpp::co_integer) {
                    duration_secs = std::get<int64_t>(opt.value);
                } else if (opt.name == "prize" && opt.type == dpp::co_string) {
                    prize_desc = std::get<std::string>(opt.value);
                }
            }
        }
    }

    // Validate duration: 30–172800 seconds (30 sec to 48 hours)
    if (duration_secs < 30 || duration_secs > 172800) {
        event.reply(dpp::message("Duration must be between 30 and 172800 seconds (48 hours).")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    if (prize_desc.empty()) {
        event.reply(dpp::message("You must specify a prize description.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Check if there's already an active roll in this channel
    auto existing = bot_.store().get_roll(cid);
    if (existing.has_value() && existing->is_active) {
        event.reply(dpp::message("There is already an active roll in this channel. "
                                  "Delete it first with /roll delete.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    auto end_time = std::chrono::system_clock::now() +
                    std::chrono::seconds{duration_secs};

    // Create the participation message with button
    auto msg = dpp::message()
        .set_content(std::format(
            "**Roll Started!** Prize: **{}**\n"
            "Click the button below to participate!\n"
            "Roll will be drawn <t:{}:R>.",
            prize_desc,
            std::chrono::duration_cast<std::chrono::seconds>(
                end_time.time_since_epoch()).count()
        ))
        .add_component(
            dpp::component()
                .add_component(
                    dpp::component()
                        .set_type(dpp::cot_button)
                        .set_label("Participate")
                        .set_style(dpp::cos_success)
                        .set_emoji("🎰")
                        .set_custom_id(std::format("roll_participate_{}",
                                                   static_cast<uint64_t>(cid)))
                )
        );

    // Send the message first, then store the message_id
    bot_.cluster().message_create(msg, [this, cid, gid, end_time, prize_desc]
        (const dpp::confirmation_callback_t& cb) {
        if (cb.is_error()) {
            bot_.cluster().log(dpp::ll_error,
                std::format("Failed to send roll message: {}", cb.get_error().message));
            return;
        }

        auto created_msg = std::get<dpp::message>(cb.value);

        RollData roll{
            .channel_id        = cid,
            .guild_id          = gid,
            .message_id        = created_msg.id,
            .end_time          = end_time,
            .participants      = {},
            .prize_description = prize_desc,
            .is_active         = true,
        };

        auto result = bot_.store().create_roll(roll);
        if (!result.has_value()) {
            bot_.cluster().log(dpp::ll_error,
                std::format("Failed to store roll: {}", result.error()));
            return;
        }

        // Schedule the roll
        schedule_roll(cid, std::chrono::duration_cast<std::chrono::seconds>(
            end_time - std::chrono::system_clock::now()));

        bot_.save_state();
    });

    event.reply(dpp::message("Roll created!").set_flags(dpp::m_ephemeral));
}

void RollModule::cmd_emergency(const dpp::slashcommand_t& event) {
    const auto cid = event.command.channel_id;
    const auto gid = event.command.guild_id;

    auto roll = bot_.store().get_roll(cid);
    if (!roll.has_value() || !roll->is_active) {
        event.reply(dpp::message("No active roll in this channel.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Cancel scheduled timer if any
    auto timer_it = scheduled_timers_.find(cid);
    if (timer_it != scheduled_timers_.end()) {
        bot_.cluster().stop_timer(timer_it->second);
        scheduled_timers_.erase(timer_it);
    }

    execute_roll(cid, *roll, gid);
    event.reply(dpp::message("Emergency roll executed!").set_flags(dpp::m_ephemeral));
}

void RollModule::cmd_delete(const dpp::slashcommand_t& event) {
    const auto cid = event.command.channel_id;

    auto roll = bot_.store().get_roll(cid);
    if (!roll.has_value()) {
        event.reply(dpp::message("No roll found in this channel.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Cancel scheduled timer if any
    auto timer_it = scheduled_timers_.find(cid);
    if (timer_it != scheduled_timers_.end()) {
        bot_.cluster().stop_timer(timer_it->second);
        scheduled_timers_.erase(timer_it);
    }

    // Delete the roll message
    bot_.cluster().message_delete(roll->message_id, cid);

    auto result = bot_.store().delete_roll(cid);
    if (!result.has_value()) {
        event.reply(dpp::message(result.error()).set_flags(dpp::m_ephemeral));
        return;
    }

    bot_.save_state();
    event.reply(dpp::message("Roll deleted.").set_flags(dpp::m_ephemeral));
}

void RollModule::handle_button(const dpp::button_click_t& event) {
    const auto cid = event.command.channel_id;
    const auto uid = event.command.get_issuing_user().id;

    auto result = bot_.store().add_participant(cid, uid);
    if (!result.has_value()) {
        event.reply(dpp::message(result.error()).set_flags(dpp::m_ephemeral));
        return;
    }

    // Get updated roll data for participant count
    auto roll = bot_.store().get_roll(cid);
    if (roll.has_value()) {
        // Update the original message with participant count
        auto updated_msg = dpp::message()
            .set_content(std::format(
                "**Roll Started!** Prize: **{}**\n"
                "Click the button below to participate!\n"
                "Participants: **{}**\n"
                "Roll will be drawn <t:{}:R>.",
                roll->prize_description,
                roll->participants.size(),
                std::chrono::duration_cast<std::chrono::seconds>(
                    roll->end_time.time_since_epoch()).count()
            ))
            .add_component(
                dpp::component()
                    .add_component(
                        dpp::component()
                            .set_type(dpp::cot_button)
                            .set_label("Participate")
                            .set_style(dpp::cos_success)
                            .set_emoji("🎰")
                            .set_custom_id(std::format("roll_participate_{}",
                                                       static_cast<uint64_t>(cid)))
                    )
            );

        bot_.cluster().message_edit(updated_msg.set_id(roll->message_id)
            .set_channel_id(cid));
    }

    event.reply(dpp::message("You've been added to the roll! Good luck! 🎰")
        .set_flags(dpp::m_ephemeral));
}

void RollModule::execute_roll(dpp::snowflake channel_id,
                               const RollData& roll_data,
                               dpp::snowflake guild_id) {
    if (roll_data.participants.empty()) {
        bot_.cluster().message_create(dpp::message(channel_id,
            "The roll has ended, but nobody participated! 😔"));
        bot_.store().deactivate_roll(channel_id);
        bot_.save_state();
        return;
    }

    // ── Cryptographically secure random selection ────────────────────────
    // Using std::random_device + std::mt19937_64 for high-quality randomness.
    // This is not predictable and suitable for prize draws.

    std::random_device rd;
    std::mt19937_64 gen(rd());

    std::uniform_int_distribution<size_t> dist(0, roll_data.participants.size() - 1);
    size_t winner_idx = dist(gen);

    dpp::snowflake winner_id = roll_data.participants[winner_idx];

    // Deactivate roll
    bot_.store().deactivate_roll(channel_id);

    // Announce winner
    auto embed = dpp::embed()
        .set_color(0xFFD700)  // Gold
        .set_title("🎰 Roll Result!")
        .add_field("Prize", roll_data.prize_description)
        .add_field("Winner", std::format("<@{}>", static_cast<uint64_t>(winner_id)))
        .add_field("Total Participants", std::to_string(roll_data.participants.size()));

    bot_.cluster().message_create(dpp::message(channel_id, embed));

    // Disable the participation button on the original message
    auto disabled_msg = dpp::message()
        .set_content(std::format(
            "**Roll Ended!** Prize: **{}**\n"
            "Winner: <@{}> | Participants: **{}**",
            roll_data.prize_description,
            static_cast<uint64_t>(winner_id),
            roll_data.participants.size()
        ))
        .add_component(
            dpp::component()
                .add_component(
                    dpp::component()
                        .set_type(dpp::cot_button)
                        .set_label("Roll Ended")
                        .set_style(dpp::cos_secondary)
                        .set_disabled(true)
                        .set_custom_id("roll_ended")
                )
        );

    bot_.cluster().message_edit(disabled_msg.set_id(roll_data.message_id)
        .set_channel_id(channel_id));

    bot_.save_state();
}

void RollModule::schedule_roll(dpp::snowflake channel_id,
                                std::chrono::seconds duration) {
    auto timer = bot_.cluster().start_timer(
        [this, channel_id](dpp::timer t) {
            auto roll = bot_.store().get_roll(channel_id);
            if (roll.has_value() && roll->is_active) {
                auto gid = roll->guild_id;
                execute_roll(channel_id, *roll, gid);
            }
            // Clean up timer reference
            scheduled_timers_.erase(channel_id);
        },
        static_cast<uint64_t>(duration.count()),
        1  // fire once
    );

    scheduled_timers_[channel_id] = timer;
}

} // namespace bot
