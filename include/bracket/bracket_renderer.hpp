#pragma once

#include <expected>
#include <string>
#include <vector>
#include "store.hpp"

namespace bot {

/// Renders a tournament bracket as a PNG image.
///
/// Uses a minimal bitmap font for text rendering and stb_image_write
/// for PNG output. No external font libraries needed.
class BracketRenderer {
public:
    BracketRenderer();

    /// Render the bracket for the given lobby and save as PNG.
    /// @return Path to the generated PNG file, or an error string.
    [[nodiscard]] auto render(const Lobby& lobby) -> std::expected<std::string, std::string>;

private:
    /// Simple RGBA canvas for drawing.
    struct Canvas {
        int width{0};
        int height{0};
        std::vector<uint8_t> pixels;  // RGBA

        Canvas(int w, int h);
        void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        void draw_rect(int x, int y, int w, int h,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        void draw_rect_outline(int x, int y, int w, int h,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        void draw_line(int x1, int y1, int x2, int y2,
                       uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        void draw_text(int x, int y, const std::string& text,
                       uint8_t r, uint8_t g, uint8_t b);
        void draw_text_centered(int cx, int cy, const std::string& text,
                                uint8_t r, uint8_t g, uint8_t b);

        auto save_png(const std::string& path) const -> bool;
    };

    /// Minimal 5x7 bitmap font for ASCII characters (printable range 32-126).
    /// Each glyph is 5 bytes (7 rows, 5 bits per row packed into bytes with MSB first).
    static const uint8_t font_data[][5];

    /// Background color
    static constexpr uint8_t BG_R = 30, BG_G = 30, BG_B = 46;
    /// Match box background
    static constexpr uint8_t BOX_R = 44, BOX_G = 47, BOX_B = 73;
    /// Active match box
    static constexpr uint8_t ACTIVE_R = 60, ACTIVE_G = 80, ACTIVE_B = 120;
    /// Text color
    static constexpr uint8_t TXT_R = 205, TXT_G = 214, TXT_B = 244;
    /// Winner text color (gold)
    static constexpr uint8_t WIN_R = 249, WIN_G = 226, TXT_B_WIN = 175;
    /// Line color
    static constexpr uint8_t LINE_R = 108, LINE_G = 112, LINE_B = 134;
};

} // namespace bot
