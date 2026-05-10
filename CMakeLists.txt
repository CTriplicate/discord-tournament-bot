cmake_minimum_required(VERSION 3.20)
project(discord_tournament_bot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────────────────────

include(FetchContent)

# D++ (Discord API wrapper)
FetchContent_Declare(
    dpp
    GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
    GIT_TAG        v10.0.35
)
FetchContent_MakeAvailable(dpp)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ── stb_image_write (header-only, vendored) ───────────────────────────────────
add_library(stb_image_write INTERFACE)
target_include_directories(stb_image_write INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party
)

# ── Bot library ───────────────────────────────────────────────────────────────

set(BOT_SOURCES
    src/config.cpp
    src/store.cpp
    src/bot.cpp
    src/modules/warning_module.cpp
    src/modules/roll_module.cpp
    src/modules/message_module.cpp
    src/modules/tournament_module.cpp
    src/bracket/bracket_renderer.cpp
)

set(BOT_HEADERS
    include/config.hpp
    include/store.hpp
    include/bot.hpp
    include/modules/warning_module.hpp
    include/modules/roll_module.hpp
    include/modules/message_module.hpp
    include/modules/tournament_module.hpp
    include/bracket/bracket_renderer.hpp
)

add_library(botlib STATIC ${BOT_SOURCES} ${BOT_HEADERS})

target_include_directories(botlib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(botlib PUBLIC
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Debug>:-g3 -fsanitize=address,undefined>
)

target_link_libraries(botlib PUBLIC
    dpp::dpp
    nlohmann_json::nlohmann_json
    stb_image_write
    stdc++fs
)

# ── Executable ────────────────────────────────────────────────────────────────

add_executable(discord_bot src/main.cpp)
target_link_libraries(discord_bot PRIVATE botlib)

# ── Install ───────────────────────────────────────────────────────────────────

install(TARGETS discord_bot RUNTIME DESTINATION bin)
install(DIRECTORY config.example.json DESTINATION etc RENAME config.json)
