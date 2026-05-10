#include "modules/warning_module.hpp"
#include "bot.hpp"

#include <dpp/dpp.h>
#include <format>
#include <sstream>

namespace bot {

WarningModule::WarningModule(Bot& bot)
    : bot_(bot) {}

void WarningModule::handle_command(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;

    // Admin check
    if (!bot_.is_admin(gid, event.command.get_issuing_user())) {
        event.reply(dpp::message("You don't have permission to use this command.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Determine subcommand
    auto sub = std::string{};
    for (const auto& opt : event.command.get_options()) {
        if (opt.type == dpp::co_sub_command) {
            sub = opt.name;
            break;
        }
    }

    // Also check top-level subcommand via interaction
    if (sub.empty()) {
        // DPP sometimes structures it differently
        const auto& cmd_data = event.command;
        for (const auto& [name, val] : cmd_data.options) {
            if (val.is_subcommand()) {
                sub = name;
                break;
            }
        }
    }

    if (sub == "add") {
        cmd_add(event);
    } else if (sub == "remove") {
        cmd_remove(event);
    } else if (sub == "set") {
        cmd_set(event);
    } else if (sub == "list") {
        cmd_list(event);
    } else {
        event.reply("Unknown warning subcommand.");
    }
}

void WarningModule::cmd_add(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;

    // Extract user from subcommand options
    dpp::snowflake target_user{0};
    std::string reason = "No reason specified";

    auto subcmds = event.command.get_options();
    for (const auto& sc : subcmds) {
        if (sc.name == "add") {
            for (const auto& opt : sc.options) {
                if (opt.name == "user" && opt.type == dpp::co_user) {
                    target_user = std::get<dpp::snowflake>(opt.value);
                } else if (opt.name == "reason" && opt.type == dpp::co_string) {
                    reason = std::get<std::string>(opt.value);
                }
            }
        }
    }

    if (target_user == dpp::snowflake{0}) {
        event.reply(dpp::message("You must specify a user.").set_flags(dpp::m_ephemeral));
        return;
    }

    int new_count = bot_.store().add_warning(gid, target_user, reason);

    // Assign warning roles
    assign_warning_roles(gid, target_user);

    // Auto-save
    bot_.save_state();

    // Build response embed
    auto embed = dpp::embed()
        .set_color(0xFF6600)
        .set_title("Warning Issued")
        .add_field("User", std::format("<@{}>", static_cast<uint64_t>(target_user)))
        .add_field("Total Warnings", std::to_string(new_count))
        .add_field("Reason", reason);

    event.reply(embed);
}

void WarningModule::cmd_remove(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;

    dpp::snowflake target_user{0};

    auto subcmds = event.command.get_options();
    for (const auto& sc : subcmds) {
        if (sc.name == "remove") {
            for (const auto& opt : sc.options) {
                if (opt.name == "user" && opt.type == dpp::co_user) {
                    target_user = std::get<dpp::snowflake>(opt.value);
                }
            }
        }
    }

    if (target_user == dpp::snowflake{0}) {
        event.reply(dpp::message("You must specify a user.").set_flags(dpp::m_ephemeral));
        return;
    }

    auto result = bot_.store().remove_warning(gid, target_user);
    if (!result.has_value()) {
        event.reply(dpp::message(result.error()).set_flags(dpp::m_ephemeral));
        return;
    }

    int remaining = result.value();

    // If no warnings left, remove the roles
    if (remaining == 0) {
        remove_warning_roles(gid, target_user);
    }

    bot_.save_state();

    auto embed = dpp::embed()
        .set_color(0x00CC00)
        .set_title("Warning Removed")
        .add_field("User", std::format("<@{}>", static_cast<uint64_t>(target_user)))
        .add_field("Remaining Warnings", std::to_string(remaining));

    event.reply(embed);
}

void WarningModule::cmd_set(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;

    std::string roles_str;

    auto subcmds = event.command.get_options();
    for (const auto& sc : subcmds) {
        if (sc.name == "set") {
            for (const auto& opt : sc.options) {
                if (opt.name == "roles" && opt.type == dpp::co_string) {
                    roles_str = std::get<std::string>(opt.value);
                }
            }
        }
    }

    if (roles_str.empty()) {
        event.reply(dpp::message("You must specify at least one role ID.")
            .set_flags(dpp::m_ephemeral));
        return;
    }

    // Parse comma-separated role IDs
    WarningConfig wc;
    std::istringstream ss(roles_str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // Trim whitespace
        auto start = token.find_first_not_of(" \t");
        auto end = token.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            auto trimmed = token.substr(start, end - start + 1);
            try {
                wc.warning_roles.emplace_back(std::stoull(trimmed));
            } catch (const std::exception&) {
                event.reply(dpp::message(
                    std::format("Invalid role ID: '{}'", trimmed))
                    .set_flags(dpp::m_ephemeral));
                return;
            }
        }
    }

    bot_.config().warning_configs[gid] = std::move(wc);
    bot_.save_state();

    // Build response
    auto& cfg = bot_.config().warning_configs[gid];
    std::string roles_list;
    for (size_t i = 0; i < cfg.warning_roles.size(); ++i) {
        if (i > 0) roles_list += ", ";
        roles_list += std::format("<@&{}>", static_cast<uint64_t>(cfg.warning_roles[i]));
    }

    auto embed = dpp::embed()
        .set_color(0x3366FF)
        .set_title("Warning Roles Updated")
        .add_field("Roles", roles_list.empty() ? "None" : roles_list);

    event.reply(embed);
}

void WarningModule::cmd_list(const dpp::slashcommand_t& event) {
    const auto gid = event.command.guild_id;
    auto warnings = bot_.store().list_warnings(gid);

    if (warnings.empty()) {
        event.reply(dpp::embed()
            .set_color(0x999999)
            .set_title("Warning List")
            .set_description("No warned users in this guild."));
        return;
    }

    auto embed = dpp::embed()
        .set_color(0xFF6600)
        .set_title("Warning List");

    for (const auto& wr : warnings) {
        embed.add_field(
            std::format("<@{}> ({} warnings)",
                       static_cast<uint64_t>(wr.user_id), wr.count),
            std::format("Last reason: {}", wr.reason),
            false  // inline
        );
    }

    event.reply(embed);
}

void WarningModule::assign_warning_roles(dpp::snowflake guild_id,
                                          dpp::snowflake user_id) {
    auto it = bot_.config().warning_configs.find(guild_id);
    if (it == bot_.config().warning_configs.end()) return;

    for (const auto& role_id : it->second.warning_roles) {
        bot_.cluster().guild_member_add_role(guild_id, user_id, role_id);
    }
}

void WarningModule::remove_warning_roles(dpp::snowflake guild_id,
                                          dpp::snowflake user_id) {
    auto it = bot_.config().warning_configs.find(guild_id);
    if (it == bot_.config().warning_configs.end()) return;

    for (const auto& role_id : it->second.warning_roles) {
        bot_.cluster().guild_member_remove_role(guild_id, user_id, role_id);
    }
}

} // namespace bot
