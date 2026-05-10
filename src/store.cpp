#include "store.hpp"

#include <fstream>
#include <shared_mutex>

namespace bot {

// ── JSON serialization helpers ───────────────────────────────────────────────

static auto snowflake_to_string(dpp::snowflake id) -> std::string {
    return std::to_string(static_cast<uint64_t>(id));
}

static auto string_to_snowflake(const std::string& s) -> dpp::snowflake {
    return dpp::snowflake{std::stoull(s)};
}

// ── WarningRecord ────────────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const WarningRecord& wr) {
    j = nlohmann::json{
        {"user_id",  snowflake_to_string(wr.user_id)},
        {"guild_id", snowflake_to_string(wr.guild_id)},
        {"count",    wr.count},
        {"reason",   wr.reason},
    };
}

void from_json(const nlohmann::json& j, WarningRecord& wr) {
    wr.user_id  = string_to_snowflake(j.at("user_id").get<std::string>());
    wr.guild_id = string_to_snowflake(j.at("guild_id").get<std::string>());
    wr.count    = j.at("count").get<int>();
    wr.reason   = j.value("reason", "");
}

// ── RollData ─────────────────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const RollData& rd) {
    auto parts = nlohmann::json::array();
    for (const auto& p : rd.participants) {
        parts.push_back(snowflake_to_string(p));
    }
    // Store end_time as epoch seconds
    auto end_secs = std::chrono::duration_cast<std::chrono::seconds>(
        rd.end_time.time_since_epoch()).count();

    j = nlohmann::json{
        {"channel_id",         snowflake_to_string(rd.channel_id)},
        {"guild_id",           snowflake_to_string(rd.guild_id)},
        {"message_id",         snowflake_to_string(rd.message_id)},
        {"end_time_epoch",     end_secs},
        {"participants",       parts},
        {"prize_description",  rd.prize_description},
        {"is_active",          rd.is_active},
    };
}

void from_json(const nlohmann::json& j, RollData& rd) {
    rd.channel_id        = string_to_snowflake(j.at("channel_id").get<std::string>());
    rd.guild_id          = string_to_snowflake(j.at("guild_id").get<std::string>());
    rd.message_id        = string_to_snowflake(j.at("message_id").get<std::string>());
    rd.prize_description = j.value("prize_description", "");
    rd.is_active         = j.value("is_active", true);

    auto end_secs = j.at("end_time_epoch").get<int64_t>();
    rd.end_time = std::chrono::system_clock::time_point{
        std::chrono::seconds{end_secs}
    };

    rd.participants.clear();
    if (j.contains("participants") && j["participants"].is_array()) {
        for (const auto& p : j["participants"]) {
            rd.participants.push_back(string_to_snowflake(p.get<std::string>()));
        }
    }
}

// ── Team ─────────────────────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const Team& t) {
    auto members = nlohmann::json::array();
    for (const auto& m : t.members) {
        members.push_back(snowflake_to_string(m));
    }
    j = nlohmann::json{
        {"name",          t.name},
        {"members",       members},
        {"is_eliminated", t.is_eliminated},
    };
}

void from_json(const nlohmann::json& j, Team& t) {
    t.name          = j.at("name").get<std::string>();
    t.is_eliminated = j.value("is_eliminated", false);

    t.members.clear();
    if (j.contains("members") && j["members"].is_array()) {
        for (const auto& m : j["members"]) {
            t.members.push_back(string_to_snowflake(m.get<std::string>()));
        }
    }
}

// ── Match ────────────────────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const Match& m) {
    j = nlohmann::json{
        {"match_id",  m.match_id},
        {"round",     m.round},
        {"position",  m.position},
        {"status",    m.status},
    };
    if (m.team1_name.has_value()) j["team1_name"] = *m.team1_name;
    if (m.team2_name.has_value()) j["team2_name"] = *m.team2_name;
    if (m.winner_name.has_value()) j["winner_name"] = *m.winner_name;
}

void from_json(const nlohmann::json& j, Match& m) {
    m.match_id = j.at("match_id").get<std::string>();
    m.round    = j.at("round").get<int>();
    m.position = j.at("position").get<int>();
    m.status   = j.at("status").get<MatchStatus>();

    m.team1_name  = j.contains("team1_name")
                        ? std::optional{j.at("team1_name").get<std::string>()}
                        : std::nullopt;
    m.team2_name  = j.contains("team2_name")
                        ? std::optional{j.at("team2_name").get<std::string>()}
                        : std::nullopt;
    m.winner_name = j.contains("winner_name")
                        ? std::optional{j.at("winner_name").get<std::string>()}
                        : std::nullopt;
}

// ── Lobby ────────────────────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const Lobby& l) {
    j = nlohmann::json{
        {"guild_id",             snowflake_to_string(l.guild_id)},
        {"channel_id",           snowflake_to_string(l.channel_id)},
        {"creator_id",           snowflake_to_string(l.creator_id)},
        {"lobby_name",           l.lobby_name},
        {"team_size",            l.team_size},
        {"format",               l.format},
        {"teams",                l.teams},
        {"matches",              l.matches},
        {"is_bracket_generated", l.is_bracket_generated},
        {"questionnaire",        l.questionnaire},
    };
}

void from_json(const nlohmann::json& j, Lobby& l) {
    l.guild_id             = string_to_snowflake(j.at("guild_id").get<std::string>());
    l.channel_id           = string_to_snowflake(j.at("channel_id").get<std::string>());
    l.creator_id           = string_to_snowflake(j.at("creator_id").get<std::string>());
    l.lobby_name           = j.at("lobby_name").get<std::string>();
    l.team_size            = j.at("team_size").get<int>();
    l.format               = j.at("format").get<TournamentFormat>();
    l.is_bracket_generated = j.value("is_bracket_generated", false);
    l.questionnaire        = j.value("questionnaire", "");

    l.teams.clear();
    if (j.contains("teams") && j["teams"].is_array()) {
        l.teams = j.at("teams").get<std::vector<Team>>();
    }

    l.matches.clear();
    if (j.contains("matches") && j["matches"].is_array()) {
        l.matches = j.at("matches").get<std::vector<Match>>();
    }
}

// ── Store implementation ─────────────────────────────────────────────────────

Store::Store(std::filesystem::path db_path)
    : db_path_(std::move(db_path)) {}

// ── Warnings ─────────────────────────────────────────────────────────────────

auto Store::add_warning(dpp::snowflake guild_id,
                         dpp::snowflake user_id,
                         const std::string& reason) -> int {
    std::lock_guard lock(mutex_);
    auto& guild_warnings = warnings_[guild_id];
    auto it = guild_warnings.find(user_id);
    if (it == guild_warnings.end()) {
        WarningRecord wr{
            .user_id  = user_id,
            .guild_id = guild_id,
            .count    = 1,
            .reason   = reason,
        };
        guild_warnings[user_id] = wr;
        return 1;
    }
    it->second.count++;
    it->second.reason = reason;
    return it->second.count;
}

auto Store::remove_warning(dpp::snowflake guild_id,
                             dpp::snowflake user_id) -> std::expected<int, std::string> {
    std::lock_guard lock(mutex_);
    auto git = warnings_.find(guild_id);
    if (git == warnings_.end()) {
        return std::unexpected("No warnings found for this guild");
    }
    auto uit = git->second.find(user_id);
    if (uit == git->second.end()) {
        return std::unexpected("User has no warnings");
    }
    uit->second.count--;
    if (uit->second.count <= 0) {
        git->second.erase(uit);
        return 0;
    }
    return uit->second.count;
}

auto Store::get_warning_count(dpp::snowflake guild_id,
                               dpp::snowflake user_id) const -> int {
    std::lock_guard lock(mutex_);
    auto git = warnings_.find(guild_id);
    if (git == warnings_.end()) return 0;
    auto uit = git->second.find(user_id);
    if (uit == git->second.end()) return 0;
    return uit->second.count;
}

auto Store::list_warnings(dpp::snowflake guild_id) const
    -> std::vector<WarningRecord> {
    std::lock_guard lock(mutex_);
    std::vector<WarningRecord> result;
    auto git = warnings_.find(guild_id);
    if (git == warnings_.end()) return result;

    result.reserve(git->second.size());
    for (const auto& [_, wr] : git->second) {
        result.push_back(wr);
    }
    return result;
}

// ── Rolls ────────────────────────────────────────────────────────────────────

auto Store::create_roll(const RollData& roll) -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    auto it = rolls_.find(roll.channel_id);
    if (it != rolls_.end() && it->second.is_active) {
        return std::unexpected("An active roll already exists in this channel");
    }
    rolls_[roll.channel_id] = roll;
    return {};
}

auto Store::get_roll(dpp::snowflake channel_id) const
    -> std::optional<RollData> {
    std::lock_guard lock(mutex_);
    auto it = rolls_.find(channel_id);
    if (it == rolls_.end()) return std::nullopt;
    return it->second;
}

auto Store::add_participant(dpp::snowflake channel_id,
                             dpp::snowflake user_id) -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    auto it = rolls_.find(channel_id);
    if (it == rolls_.end() || !it->second.is_active) {
        return std::unexpected("No active roll in this channel");
    }

    // Check if already participating
    auto& parts = it->second.participants;
    if (std::ranges::find(parts, user_id) != parts.end()) {
        return std::unexpected("You are already participating in this roll");
    }

    parts.push_back(user_id);
    return {};
}

auto Store::deactivate_roll(dpp::snowflake channel_id) -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    auto it = rolls_.find(channel_id);
    if (it == rolls_.end()) {
        return std::unexpected("No roll found in this channel");
    }
    it->second.is_active = false;
    return {};
}

auto Store::delete_roll(dpp::snowflake channel_id) -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    auto it = rolls_.find(channel_id);
    if (it == rolls_.end()) {
        return std::unexpected("No roll found in this channel");
    }
    rolls_.erase(it);
    return {};
}

// ── Lobbies / Tournaments ────────────────────────────────────────────────────

auto Store::create_lobby(const Lobby& lobby) -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    auto& guild_lobbies = lobbies_[lobby.guild_id];
    if (guild_lobbies.contains(lobby.lobby_name)) {
        return std::unexpected(
            std::format("Lobby '{}' already exists", lobby.lobby_name));
    }
    guild_lobbies[lobby.lobby_name] = lobby;
    return {};
}

auto Store::get_lobby(dpp::snowflake guild_id,
                       const std::string& name) const
    -> std::optional<Lobby> {
    std::lock_guard lock(mutex_);
    auto git = lobbies_.find(guild_id);
    if (git == lobbies_.end()) return std::nullopt;
    auto lit = git->second.find(name);
    if (lit == git->second.end()) return std::nullopt;
    return lit->second;
}

auto Store::list_lobbies(dpp::snowflake guild_id) const
    -> std::vector<Lobby> {
    std::lock_guard lock(mutex_);
    std::vector<Lobby> result;
    auto git = lobbies_.find(guild_id);
    if (git == lobbies_.end()) return result;

    result.reserve(git->second.size());
    for (const auto& [_, lobby] : git->second) {
        result.push_back(lobby);
    }
    return result;
}

auto Store::delete_lobby(dpp::snowflake guild_id,
                          const std::string& name) -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    auto git = lobbies_.find(guild_id);
    if (git == lobbies_.end()) {
        return std::unexpected("No lobbies found in this guild");
    }
    auto lit = git->second.find(name);
    if (lit == git->second.end()) {
        return std::unexpected(std::format("Lobby '{}' not found", name));
    }
    git->second.erase(lit);
    return {};
}

auto Store::add_team(dpp::snowflake guild_id,
                      const std::string& lobby_name,
                      Team team) -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    auto git = lobbies_.find(guild_id);
    if (git == lobbies_.end()) {
        return std::unexpected("No lobbies found in this guild");
    }
    auto lit = git->second.find(lobby_name);
    if (lit == git->second.end()) {
        return std::unexpected(std::format("Lobby '{}' not found", lobby_name));
    }

    auto& lobby = lit->second;
    // Check team name uniqueness
    for (const auto& t : lobby.teams) {
        if (t.name == team.name) {
            return std::unexpected(
                std::format("Team '{}' already exists in lobby '{}'",
                            team.name, lobby_name));
        }
    }

    // Validate team size
    if (static_cast<int>(team.members.size()) > lobby.team_size) {
        return std::unexpected(
            std::format("Team has {} members but lobby requires max {}",
                        team.members.size(), lobby.team_size));
    }

    lobby.teams.push_back(std::move(team));
    return {};
}

auto Store::delete_team(dpp::snowflake guild_id,
                         const std::string& lobby_name,
                         const std::string& team_name) -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    auto git = lobbies_.find(guild_id);
    if (git == lobbies_.end()) {
        return std::unexpected("No lobbies found in this guild");
    }
    auto lit = git->second.find(lobby_name);
    if (lit == git->second.end()) {
        return std::unexpected(std::format("Lobby '{}' not found", lobby_name));
    }

    auto& teams = lit->second.teams;
    auto tit = std::ranges::find_if(teams,
        [&team_name](const Team& t) { return t.name == team_name; });
    if (tit == teams.end()) {
        return std::unexpected(
            std::format("Team '{}' not found in lobby '{}'",
                        team_name, lobby_name));
    }
    teams.erase(tit);
    return {};
}

auto Store::update_lobby(dpp::snowflake guild_id,
                          const Lobby& lobby) -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    auto git = lobbies_.find(guild_id);
    if (git == lobbies_.end()) {
        return std::unexpected("No lobbies found in this guild");
    }
    auto lit = git->second.find(lobby.lobby_name);
    if (lit == git->second.end()) {
        return std::unexpected(
            std::format("Lobby '{}' not found", lobby.lobby_name));
    }
    lit->second = lobby;
    return {};
}

// ── Persistence ──────────────────────────────────────────────────────────────

auto Store::save() -> std::expected<void, std::string> {
    std::lock_guard lock(mutex_);
    try {
        auto parent = db_path_.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }

        nlohmann::json j;

        // Serialize warnings
        auto warnings_json = nlohmann::json::object();
        for (const auto& [gid, guild_warnings] : warnings_) {
            auto gw = nlohmann::json::object();
            for (const auto& [uid, wr] : guild_warnings) {
                gw[snowflake_to_string(uid)] = wr;
            }
            warnings_json[snowflake_to_string(gid)] = gw;
        }
        j["warnings"] = warnings_json;

        // Serialize rolls (only active ones)
        auto rolls_json = nlohmann::json::object();
        for (const auto& [cid, rd] : rolls_) {
            if (rd.is_active) {
                rolls_json[snowflake_to_string(cid)] = rd;
            }
        }
        j["rolls"] = rolls_json;

        // Serialize lobbies
        auto lobbies_json = nlohmann::json::object();
        for (const auto& [gid, guild_lobbies] : lobbies_) {
            auto gl = nlohmann::json::object();
            for (const auto& [name, lobby] : guild_lobbies) {
                gl[name] = lobby;
            }
            lobbies_json[snowflake_to_string(gid)] = gl;
        }
        j["lobbies"] = lobbies_json;

        std::ofstream ofs(db_path_);
        if (!ofs.is_open()) {
            return std::unexpected(
                std::format("Cannot open database for writing: {}",
                            db_path_.string()));
        }
        ofs << j.dump(2);
        return {};
    } catch (const std::exception& e) {
        return std::unexpected(
            std::format("Failed to save database: {}", e.what()));
    }
}

auto Store::load() -> std::expected<void, std::string> {
    if (!std::filesystem::exists(db_path_)) {
        // Fresh start, no data file yet
        return {};
    }

    try {
        std::ifstream ifs(db_path_);
        if (!ifs.is_open()) {
            return std::unexpected(
                std::format("Cannot open database: {}", db_path_.string()));
        }

        auto j = nlohmann::json::parse(ifs);
        std::lock_guard lock(mutex_);

        // Load warnings
        warnings_.clear();
        if (j.contains("warnings") && j["warnings"].is_object()) {
            for (const auto& [gid_str, gw_json] : j["warnings"].items()) {
                auto gid = string_to_snowflake(gid_str);
                for (const auto& [uid_str, wr_json] : gw_json.items()) {
                    auto uid = string_to_snowflake(uid_str);
                    warnings_[gid][uid] = wr_json.get<WarningRecord>();
                }
            }
        }

        // Load rolls
        rolls_.clear();
        if (j.contains("rolls") && j["rolls"].is_object()) {
            for (const auto& [cid_str, rd_json] : j["rolls"].items()) {
                auto cid = string_to_snowflake(cid_str);
                rolls_[cid] = rd_json.get<RollData>();
            }
        }

        // Load lobbies
        lobbies_.clear();
        if (j.contains("lobbies") && j["lobbies"].is_object()) {
            for (const auto& [gid_str, gl_json] : j["lobbies"].items()) {
                auto gid = string_to_snowflake(gid_str);
                for (const auto& [name, lobby_json] : gl_json.items()) {
                    lobbies_[gid][name] = lobby_json.get<Lobby>();
                }
            }
        }

        return {};
    } catch (const nlohmann::json::exception& e) {
        return std::unexpected(
            std::format("JSON parse error in database: {}", e.what()));
    } catch (const std::exception& e) {
        return std::unexpected(
            std::format("Failed to load database: {}", e.what()));
    }
}

} // namespace bot
