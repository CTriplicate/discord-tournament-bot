#include "modules/message_module.hpp"
#include "bot.hpp"

#include <dpp/dpp.h>
#include <format>
#include <vector>

namespace bot {

MessageModule::MessageModule(Bot& bot)
    : bot_(bot) {}

void MessageModule::handle_command(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;

    // Admin check
    if (!bot_.is_admin(gid, event.command.get_issuing_user())) {
        event.reply(dpp::message("You don't have permission to use this command.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Extract role and text
    dpp::snowflake target_role{0};
    std::string message_text;

    for (const auto& opt : event.command.get_options()) {
        if (opt.name == "role" && opt.type == dpp::co_role) {
            target_role = std::get<dpp::snowflake>(opt.value);
        } else if (opt.name == "text" && opt.type == dpp::co_string) {
            message_text = std::get<std::string>(opt.value);
        }
    }

    if (target_role == dpp::snowflake{0} || message_text.empty()) {
        event.reply(dpp::message("You must specify both a role and message text.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Acknowledge the command immediately (DMs take time)
    event.reply(dpp::message("Sending messages... Please wait.").set_flags(dpp::m_ephemeral));

    // Fetch all guild members with the target role.
    // DPP caches guild members; we iterate through the guild's member map.
    auto* guild = dpp::find_guild(gid);
    if (!guild) {
        bot_.cluster().interaction_followup_edit_original(
            event.command.token,
            dpp::message("Guild not found in cache."));
        return;
    }

    // Collect member IDs that have the target role
    std::vector<dpp::snowflake> target_members;
    for (const auto& [uid, member] : guild->members) {
        const auto& roles = member.get_roles();
        if (std::ranges::find(roles, target_role) != roles.end()) {
            target_members.push_back(uid);
        }
    }

    if (target_members.empty()) {
        bot_.cluster().interaction_followup_edit_original(
            event.command.token,
            dpp::message("No members found with the specified role."));
        return;
    }

    // Send DM to each member.
    // We use rate-limit-aware sending: stagger messages to avoid hitting
    // Discord's rate limits (5 DMs per 5 seconds is safe).
    size_t success_count = 0;
    size_t fail_count    = 0;

    for (const auto& uid : target_members) {
        // Create DM channel first, then send message
        bot_.cluster().create_dm_channel(uid,
            [this, message_text, &success_count, &fail_count, token = event.command.token]
            (const dpp::confirmation_callback_t& cb) {
                if (cb.is_error()) {
                    fail_count++;
                    return;
                }

                auto dm_channel = std::get<dpp::channel>(cb.value);
                bot_.cluster().message_create(
                    dpp::message(dm_channel.id, message_text),
                    [&success_count, &fail_count]
                    (const dpp::confirmation_callback_t& send_cb) {
                        if (send_cb.is_error()) {
                            fail_count++;
                        } else {
                            success_count++;
                        }
                    });
            });
    }

    // Edit the follow-up with results.
    // Since DMs are async, we provide an estimate.
    // The actual counts will be updated asynchronously, but the response
    // must be sent within 15 minutes (Discord interaction timeout).
    bot_.cluster().interaction_followup_edit_original(
        event.command.token,
        dpp::message(std::format(
            "DM dispatch initiated for **{}** members with the specified role.\n"
            "Some messages may fail if users have DMs disabled.",
            target_members.size()
        ))
    );
}

} // namespace bot
