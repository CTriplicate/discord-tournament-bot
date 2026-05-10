#pragma once

#include <cstdint>
#include <chrono>
#include <expected>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>
#include <dpp/dpp.h>

namespace bot {

/// Single warning record for a user within a guild.
struct WarningRecord {
    dpp::snowflake user_id{};
    dpp::snowflake guild_id{};
    int count{0};
    std::string reason{};   // Last reason
};

/// Active roll (prize raffle) within a channel.
struct RollData {
    dpp::snowflake channel_id{};
    dpp::snowflake guild_id{};
    dpp::snowflake message_id{};
    std::chrono::system_clock::time_point end_time{};
    std::vector<dpp::snowflake> participants{};
    std::string prize_description{};
    bool is_active{true};
};

/// Team within a tournament lobby.
struct Team {
    std::string name{};
    std::vector<dpp::snowflake> members{};
    bool is_eliminated{false};
};

/// Match status within a tournament bracket.
enum class MatchStatus : uint8_t {
    Pending,
    InProgress,
    Completed,
};

/// A single match in the bracket.
struct Match {
    std::string match_id{};         // Unique ID within lobby
    int round{0};
    int position{0};                // Position within the round
    std::optional<std::string> team1_name{};
    std::optional<std::string> team2_name{};
    std::optional<std::string> winner_name{};
    MatchStatus status{MatchStatus::Pending};
};

/// Tournament format.
enum class TournamentFormat : uint8_t {
    SingleElimination,
    DoubleElimination,
    RoundRobin,
};

/// Tournament lobby.
struct Lobby {
    dpp::snowflake guild_id{};
    dpp::snowflake channel_id{};
    dpp::snowflake creator_id{};
    std::string lobby_name{};
    int team_size{1};               // 1=1v1, 2=2v2, 3=3v3, etc.
    TournamentFormat format{TournamentFormat::SingleElimination};
    std::vector<Team> teams{};
    std::vector<Match> matches{};
    bool is_bracket_generated{false};
    std::string questionnaire{};    // Criteria/questions for participants
};

// ── JSON serialization declarations ──────────────────────────────────────────

void to_json(nlohmann::json& j, const WarningRecord& wr);
void from_json(const nlohmann::json& j, WarningRecord& wr);

void to_json(nlohmann::json& j, const RollData& rd);
void from_json(const nlohmann::json& j, RollData& rd);

void to_json(nlohmann::json& j, const Team& t);
void from_json(const nlohmann::json& j, Team& t);

void to_json(nlohmann::json& j, const Match& m);
void from_json(const nlohmann::json& j, Match& m);

void to_json(nlohmann::json& j, const Lobby& l);
void from_json(const nlohmann::json& j, Lobby& l);

NLOHMANN_JSON_SERIALIZE_ENUM(MatchStatus, {
    {MatchStatus::Pending,    "pending"},
    {MatchStatus::InProgress, "in_progress"},
    {MatchStatus::Completed,  "completed"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(TournamentFormat, {
    {TournamentFormat::SingleElimination, "single_elimination"},
    {TournamentFormat::DoubleElimination, "double_elimination"},
    {TournamentFormat::RoundRobin,        "round_robin"},
})

// ── Persistent data store ────────────────────────────────────────────────────

class Store {
public:
    explicit Store(std::filesystem::path db_path);

    // ── Warnings ──────────────────────────────────────────────────────────

    [[nodiscard]] auto add_warning(dpp::snowflake guild_id,
                                    dpp::snowflake user_id,
                                    const std::string& reason) -> int;
    [[nodiscard]] auto remove_warning(dpp::snowflake guild_id,
                                       dpp::snowflake user_id) -> std::expected<int, std::string>;
    [[nodiscard]] auto get_warning_count(dpp::snowflake guild_id,
                                          dpp::snowflake user_id) const -> int;
    [[nodiscard]] auto list_warnings(dpp::snowflake guild_id) const
        -> std::vector<WarningRecord>;

    // ── Rolls ─────────────────────────────────────────────────────────────

    [[nodiscard]] auto create_roll(const RollData& roll) -> std::expected<void, std::string>;
    [[nodiscard]] auto get_roll(dpp::snowflake channel_id) const
        -> std::optional<RollData>;
    [[nodiscard]] auto add_participant(dpp::snowflake channel_id,
                                        dpp::snowflake user_id) -> std::expected<void, std::string>;
    [[nodiscard]] auto deactivate_roll(dpp::snowflake channel_id) -> std::expected<void, std::string>;
    [[nodiscard]] auto delete_roll(dpp::snowflake channel_id) -> std::expected<void, std::string>;

    // ── Lobbies / Tournaments ─────────────────────────────────────────────

    [[nodiscard]] auto create_lobby(const Lobby& lobby) -> std::expected<void, std::string>;
    [[nodiscard]] auto get_lobby(dpp::snowflake guild_id,
                                  const std::string& name) const
        -> std::optional<Lobby>;
    [[nodiscard]] auto list_lobbies(dpp::snowflake guild_id) const
        -> std::vector<Lobby>;
    [[nodiscard]] auto delete_lobby(dpp::snowflake guild_id,
                                     const std::string& name) -> std::expected<void, std::string>;

    /// Add a team to a lobby.
    [[nodiscard]] auto add_team(dpp::snowflake guild_id,
                                 const std::string& lobby_name,
                                 Team team) -> std::expected<void, std::string>;

    /// Delete a team from a lobby.
    [[nodiscard]] auto delete_team(dpp::snowflake guild_id,
                                    const std::string& lobby_name,
                                    const std::string& team_name) -> std::expected<void, std::string>;

    /// Update a lobby (after bracket generation, match updates, etc.).
    [[nodiscard]] auto update_lobby(dpp::snowflake guild_id,
                                     const Lobby& lobby) -> std::expected<void, std::string>;

    // ── Persistence ───────────────────────────────────────────────────────

    auto save() -> std::expected<void, std::string>;
    auto load() -> std::expected<void, std::string>;

private:
    std::filesystem::path db_path_;
    mutable std::mutex mutex_;

    // Key: guild_id -> user_id -> record
    std::unordered_map<dpp::snowflake,
        std::unordered_map<dpp::snowflake, WarningRecord>> warnings_;

    // Key: channel_id -> roll data (one active roll per channel)
    std::unordered_map<dpp::snowflake, RollData> rolls_;

    // Key: guild_id -> lobby_name -> lobby
    std::unordered_map<dpp::snowflake,
        std::unordered_map<std::string, Lobby>> lobbies_;
};

} // namespace bot
