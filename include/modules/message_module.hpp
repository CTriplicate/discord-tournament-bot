#pragma once

#include <dpp/dpp.h>

namespace bot {

class Bot;

/// Handles the /message command — send a DM to all members with a given role.
///
/// Command:
///   /message role:<@role> text:<message>
class MessageModule {
public:
    explicit MessageModule(Bot& bot);

    /// Dispatch the /message slash command.
    void handle_command(const dpp::slashcommand_t& event);

private:
    Bot& bot_;
};

} // namespace bot
