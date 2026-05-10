#include "bracket/bracket_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>

// stb_image_write for PNG output
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace bot {

// ── 5x7 Bitmap Font (printable ASCII 32-126) ────────────────────────────────
// Each character is 5 columns x 7 rows.
// Stored as 7 bytes per character, each byte has 5 MSB bits for the 5 columns.
// Bit 7 = leftmost column, bit 3 = rightmost column.

const uint8_t BracketRenderer::font_data[][5] = {
    // Space (32)
    {0x00, 0x00, 0x00, 0x00, 0x00},
    // !
    {0x00, 0x00, 0x5F, 0x00, 0x00},
    // "
    {0x00, 0x07, 0x00, 0x07, 0x00},
    // #
    {0x14, 0x7F, 0x14, 0x7F, 0x14},
    // $
    {0x24, 0x2A, 0x7F, 0x2A, 0x12},
    // %
    {0x23, 0x13, 0x08, 0x64, 0x62},
    // &
    {0x36, 0x49, 0x55, 0x22, 0x50},
    // '
    {0x00, 0x05, 0x03, 0x00, 0x00},
    // (
    {0x00, 0x1C, 0x22, 0x41, 0x00},
    // )
    {0x00, 0x41, 0x22, 0x1C, 0x00},
    // *
    {0x14, 0x08, 0x3E, 0x08, 0x14},
    // +
    {0x08, 0x08, 0x3E, 0x08, 0x08},
    // ,
    {0x00, 0x50, 0x30, 0x00, 0x00},
    // -
    {0x08, 0x08, 0x08, 0x08, 0x08},
    // .
    {0x00, 0x60, 0x60, 0x00, 0x00},
    // /
    {0x20, 0x10, 0x08, 0x04, 0x02},
    // 0
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    // 1
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    // 2
    {0x42, 0x61, 0x51, 0x49, 0x46},
    // 3
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    // 4
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    // 5
    {0x27, 0x45, 0x45, 0x45, 0x39},
    // 6
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    // 7
    {0x01, 0x71, 0x09, 0x05, 0x03},
    // 8
    {0x36, 0x49, 0x49, 0x49, 0x36},
    // 9
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    // :
    {0x00, 0x36, 0x36, 0x00, 0x00},
    // ;
    {0x00, 0x56, 0x36, 0x00, 0x00},
    // <
    {0x08, 0x14, 0x22, 0x41, 0x00},
    // =
    {0x14, 0x14, 0x14, 0x14, 0x14},
    // >
    {0x00, 0x41, 0x22, 0x14, 0x08},
    // ?
    {0x02, 0x01, 0x51, 0x09, 0x06},
    // @
    {0x32, 0x49, 0x79, 0x41, 0x3E},
    // A
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    // B
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    // C
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    // D
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    // E
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    // F
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    // G
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    // H
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    // I
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    // J
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    // K
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    // L
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    // M
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    // N
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    // O
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    // P
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    // Q
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    // R
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    // S
    {0x46, 0x49, 0x49, 0x49, 0x31},
    // T
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    // U
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    // V
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    // W
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    // X
    {0x63, 0x14, 0x08, 0x14, 0x63},
    // Y
    {0x07, 0x08, 0x70, 0x08, 0x07},
    // Z
    {0x61, 0x51, 0x49, 0x45, 0x43},
    // [
    {0x00, 0x7F, 0x41, 0x41, 0x00},
    // backslash
    {0x02, 0x04, 0x08, 0x10, 0x20},
    // ]
    {0x00, 0x41, 0x41, 0x7F, 0x00},
    // ^
    {0x04, 0x02, 0x01, 0x02, 0x04},
    // _
    {0x40, 0x40, 0x40, 0x40, 0x40},
    // `
    {0x00, 0x01, 0x02, 0x04, 0x00},
    // a
    {0x20, 0x54, 0x54, 0x54, 0x78},
    // b
    {0x7F, 0x48, 0x44, 0x44, 0x38},
    // c
    {0x38, 0x44, 0x44, 0x44, 0x20},
    // d
    {0x38, 0x44, 0x44, 0x48, 0x7F},
    // e
    {0x38, 0x54, 0x54, 0x54, 0x18},
    // f
    {0x08, 0x7E, 0x09, 0x01, 0x02},
    // g
    {0x0C, 0x52, 0x52, 0x52, 0x3E},
    // h
    {0x7F, 0x08, 0x04, 0x04, 0x78},
    // i
    {0x00, 0x44, 0x7D, 0x40, 0x00},
    // j
    {0x20, 0x40, 0x44, 0x3D, 0x00},
    // k
    {0x7F, 0x10, 0x28, 0x44, 0x00},
    // l
    {0x00, 0x41, 0x7F, 0x40, 0x00},
    // m
    {0x7C, 0x04, 0x18, 0x04, 0x78},
    // n
    {0x7C, 0x08, 0x04, 0x04, 0x78},
    // o
    {0x38, 0x44, 0x44, 0x44, 0x38},
    // p
    {0x7C, 0x14, 0x14, 0x14, 0x08},
    // q
    {0x08, 0x14, 0x14, 0x18, 0x7C},
    // r
    {0x7C, 0x08, 0x04, 0x04, 0x08},
    // s
    {0x48, 0x54, 0x54, 0x54, 0x20},
    // t
    {0x04, 0x3F, 0x44, 0x40, 0x20},
    // u
    {0x3C, 0x40, 0x40, 0x20, 0x7C},
    // v
    {0x1C, 0x20, 0x40, 0x20, 0x1C},
    // w
    {0x3C, 0x40, 0x30, 0x40, 0x3C},
    // x
    {0x44, 0x28, 0x10, 0x28, 0x44},
    // y
    {0x0C, 0x50, 0x50, 0x50, 0x3C},
    // z
    {0x44, 0x64, 0x54, 0x4C, 0x44},
    // {
    {0x00, 0x08, 0x36, 0x41, 0x00},
    // |
    {0x00, 0x00, 0x7F, 0x00, 0x00},
    // }
    {0x00, 0x41, 0x36, 0x08, 0x00},
    // ~
    {0x10, 0x08, 0x08, 0x10, 0x08},
};

// ── Canvas implementation ────────────────────────────────────────────────────

BracketRenderer::Canvas::Canvas(int w, int h)
    : width(w), height(h), pixels(w * h * 4, 0) {}

void BracketRenderer::Canvas::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int i = 0; i < width * height; ++i) {
        pixels[i * 4 + 0] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = a;
    }
}

void BracketRenderer::Canvas::draw_rect(int x, int y, int w, int h,
                                         uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            int px = x + dx;
            int py = y + dy;
            if (px < 0 || px >= width || py < 0 || py >= height) continue;

            int idx = (py * width + px) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }
    }
}

void BracketRenderer::Canvas::draw_rect_outline(int x, int y, int w, int h,
                                                  uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Top
    draw_line(x, y, x + w - 1, y, r, g, b, a);
    // Bottom
    draw_line(x, y + h - 1, x + w - 1, y + h - 1, r, g, b, a);
    // Left
    draw_line(x, y, x, y + h - 1, r, g, b, a);
    // Right
    draw_line(x + w - 1, y, x + w - 1, y + h - 1, r, g, b, a);
}

void BracketRenderer::Canvas::draw_line(int x1, int y1, int x2, int y2,
                                          uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Bresenham's line algorithm
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x1 >= 0 && x1 < width && y1 >= 0 && y1 < height) {
            int idx = (y1 * width + x1) * 4;
            pixels[idx + 0] = r;
            pixels[idx + 1] = g;
            pixels[idx + 2] = b;
            pixels[idx + 3] = a;
        }

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
    }
}

void BracketRenderer::Canvas::draw_text(int x, int y, const std::string& text,
                                          uint8_t r, uint8_t g, uint8_t b) {
    int cursor_x = x;
    for (char c : text) {
        int idx = static_cast<int>(c) - 32;
        if (idx < 0 || idx > 94) {
            cursor_x += 6;  // Skip unknown characters
            continue;
        }

        const auto& glyph = font_data[idx];
        for (int row = 0; row < 7; ++row) {
            // Each byte has 5 MSB bits for the 5 columns
            uint8_t row_data = glyph[row];  // Actually we stored 5 bytes per char
            // Wait, our font_data is [5] per char, but we need 7 rows...
            // Let me re-read the data structure.
            // Actually, the font_data is stored as 5 bytes, each byte is one column.
            // We need to re-interpret: each byte has 7 bits (rows), MSB = top row.
            (void)row_data;  // Will fix below
        }

        // Correct interpretation: 5 bytes per character, each byte = 1 column
        // bit 6 (0x40) = row 0 (top), bit 0 = row 6 (bottom)
        for (int col = 0; col < 5; ++col) {
            uint8_t col_data = glyph[col];
            for (int row = 0; row < 7; ++row) {
                if (col_data & (0x40 >> row)) {
                    int px = cursor_x + col;
                    int py = y + row;
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        int pidx = (py * width + px) * 4;
                        pixels[pidx + 0] = r;
                        pixels[pidx + 1] = g;
                        pixels[pidx + 2] = b;
                        pixels[pidx + 3] = 255;
                    }
                }
            }
        }

        cursor_x += 6;  // 5 pixels wide + 1 pixel spacing
    }
}

void BracketRenderer::Canvas::draw_text_centered(int cx, int cy,
                                                   const std::string& text,
                                                   uint8_t r, uint8_t g, uint8_t b) {
    // Approximate text width: 6 pixels per character
    int text_width = static_cast<int>(text.size()) * 6;
    int text_height = 7;
    draw_text(cx - text_width / 2, cy - text_height / 2, text, r, g, b);
}

auto BracketRenderer::Canvas::save_png(const std::string& path) const -> bool {
    // stb_image_write expects RGB or RGBA data
    int stride = width * 4;
    return stbi_write_png(path.c_str(), width, height, 4, pixels.data(), stride) != 0;
}

// ── BracketRenderer implementation ───────────────────────────────────────────

BracketRenderer::BracketRenderer() = default;

auto BracketRenderer::render(const Lobby& lobby) -> std::expected<std::string, std::string> {
    if (lobby.matches.empty()) {
        return std::unexpected("No matches to render");
    }

    // Calculate layout dimensions
    int max_round = 0;
    for (const auto& m : lobby.matches) {
        max_round = std::max(max_round, m.round);
    }
    int total_rounds = max_round + 1;

    // Layout constants
    constexpr int box_width     = 180;
    constexpr int box_height    = 50;
    constexpr int round_gap     = 80;   // Horizontal gap between rounds
    constexpr int match_gap     = 10;   // Vertical gap between matches
    constexpr int padding       = 40;
    constexpr int title_height  = 30;
    constexpr int scale         = 2;    // Pixel scale for better resolution

    // Calculate canvas size
    int canvas_w = padding * 2 + total_rounds * (box_width + round_gap) - round_gap;
    int canvas_h = padding * 2 + title_height +
                   static_cast<int>(std::pow(2, total_rounds - 1)) * (box_height + match_gap) - match_gap;

    canvas_w *= scale;
    canvas_h *= scale;

    Canvas canvas(canvas_w, canvas_h);
    canvas.fill(BG_R, BG_G, BG_B);

    // Draw title
    canvas.draw_text_centered(canvas_w / 2, padding * scale,
                              lobby.lobby_name, TXT_R, TXT_G, TXT_B);

    // Draw matches for each round
    for (int round = 0; round < total_rounds; ++round) {
        int matches_in_round = 1 << (total_rounds - 1 - round);
        int round_x = (padding + round * (box_width + round_gap)) * scale;

        // Vertical centering: matches spread out more in earlier rounds
        int total_match_height = matches_in_round * box_height * scale +
                                 (matches_in_round - 1) * match_gap * scale;
        int start_y = padding * scale + title_height * scale +
                      ((canvas_h - padding * 2 * scale - title_height * scale) - total_match_height) / 2;

        for (int pos = 0; pos < matches_in_round; ++pos) {
            int match_y = start_y + pos * (box_height + match_gap) * scale;

            // Find the match
            auto match_it = std::ranges::find_if(lobby.matches,
                [round, pos](const Match& m) { return m.round == round && m.position == pos; });

            if (match_it == lobby.matches.end()) continue;

            const auto& match = *match_it;

            // Draw match box
            bool is_active = (match.status == MatchStatus::InProgress);
            bool is_completed = (match.status == MatchStatus::Completed);

            uint8_t br = is_active ? ACTIVE_R : BOX_R;
            uint8_t bg = is_active ? ACTIVE_G : BOX_G;
            uint8_t bb = is_active ? ACTIVE_B : BOX_B;

            canvas.draw_rect(round_x, match_y,
                            box_width * scale, box_height * scale, br, bg, bb);
            canvas.draw_rect_outline(round_x, match_y,
                                     box_width * scale, box_height * scale,
                                     LINE_R, LINE_G, LINE_B);

            // Draw team names
            int text_y1 = match_y + 8 * scale;
            int text_y2 = match_y + 26 * scale;

            auto t1 = match.team1_name.value_or("TBD");
            auto t2 = match.team2_name.value_or("TBD");

            // Truncate long names
            if (t1.size() > 14) t1 = t1.substr(0, 13) + "..";
            if (t2.size() > 14) t2 = t2.substr(0, 13) + "..";

            // Team 1
            auto [t1r, t1g, t1b] = (is_completed && match.winner_name == match.team1_name)
                ? std::tuple{WIN_R, WIN_G, TXT_B_WIN}
                : std::tuple{TXT_R, TXT_G, TXT_B};
            canvas.draw_text(round_x + 8 * scale, text_y1, t1, t1r, t1g, t1b);

            // Separator line
            canvas.draw_line(round_x + 4 * scale, match_y + box_height * scale / 2,
                            round_x + (box_width - 4) * scale, match_y + box_height * scale / 2,
                            LINE_R, LINE_G, LINE_B);

            // Team 2
            auto [t2r, t2g, t2b] = (is_completed && match.winner_name == match.team2_name)
                ? std::tuple{WIN_R, WIN_G, TXT_B_WIN}
                : std::tuple{TXT_R, TXT_G, TXT_B};
            canvas.draw_text(round_x + 8 * scale, text_y2, t2, t2r, t2g, t2b);

            // Draw connecting lines to next round
            if (round + 1 < total_rounds) {
                int next_round_x = (padding + (round + 1) * (box_width + round_gap)) * scale;
                int next_pos = pos / 2;
                int matches_in_next = 1 << (total_rounds - 2 - round);

                int next_total_height = matches_in_next * box_height * scale +
                                        (matches_in_next - 1) * match_gap * scale;
                int next_start_y = padding * scale + title_height * scale +
                                   ((canvas_h - padding * 2 * scale - title_height * scale) - next_total_height) / 2;

                int next_match_y = next_start_y + next_pos * (box_height + match_gap) * scale;

                int from_x = round_x + box_width * scale;
                int from_y = match_y + (box_height * scale) / 2;
                int to_x = next_round_x;
                int to_y = next_match_y + (box_height * scale) / 2;

                // Horizontal line from this match
                int mid_x = from_x + (to_x - from_x) / 2;
                canvas.draw_line(from_x, from_y, mid_x, from_y, LINE_R, LINE_G, LINE_B);

                // Vertical line to next match center
                // We draw vertical later, connecting pairs
                // For now, just draw diagonal
                canvas.draw_line(mid_x, from_y, to_x, to_y, LINE_R, LINE_G, LINE_B);
            }

            // Draw match ID
            canvas.draw_text(round_x + (box_width - 8) * scale - static_cast<int>(match.match_id.size()) * 6 * scale / 2,
                            match_y + (box_height - 8) * scale,
                            match.match_id, LINE_R, LINE_G, LINE_B);
        }
    }

    // Save the image
    auto output_dir = std::filesystem::path{"data/brackets"};
    if (!std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }

    auto output_path = (output_dir / std::format("bracket_{}.png", lobby.lobby_name)).string();
    if (!canvas.save_png(output_path)) {
        return std::unexpected("Failed to save bracket PNG");
    }

    return output_path;
}

} // namespace bot
