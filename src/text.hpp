#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace rr {
class Assets;

// 32-bit 0x00RRGGBB image used by the front end (the original menus are 15/16-bit
// DirectDraw surfaces, not the 8-bit race surface).
struct RgbImage {
    int width = 0, height = 0;
    std::vector<uint32_t> pixels;
    RgbImage() = default;
    RgbImage(int w, int h, uint32_t fill = 0) : width(w), height(h), pixels(size_t(w) * size_t(h), fill) {}
    uint32_t& at(int x, int y) { return pixels[size_t(y) * size_t(width) + size_t(x)]; }
    uint32_t at(int x, int y) const { return pixels[size_t(y) * size_t(width) + size_t(x)]; }
    bool contains(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
};

struct TextRect {
    int left = 0, top = 0, right = 0, bottom = 0;
};

// Faces the original installs from TEXT/*.DLL (0x0042f720): these files are TrueType fonts.
enum class Face { badloc, futura_regular, futura_bold, futura_demi };

// Win32 DrawText flags used by the original menus.
namespace dt {
constexpr unsigned center = 0x1, right = 0x2, vcenter = 0x4, bottom = 0x8, wordbreak = 0x10,
                   singleline = 0x20, calcrect = 0x400;
}

// Text drawing with the original's own TrueType fonts, read from the user's disc at runtime.
// GDI of the period renders without anti-aliasing and advances by whole pixels; this renderer
// does the same (coverage >= 50% is ink). Exact GDI hinting is not reproduced.
class TextRenderer {
  public:
    explicit TextRenderer(const Assets&);
    TextRenderer(Face, std::vector<uint8_t> ttf); // single face, for tests
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    bool has(Face) const;
    // Strings are Windows-1252 bytes as the game uses them. height is the GDI cell height.
    int width(Face, int height, const std::string& text) const;
    int line_height(Face, int height) const;
    void draw(RgbImage&, Face, int height, int x, int y, uint32_t rgb, const std::string& text) const;
    // DrawTextEx-like layout: newlines, word wrap, left/centre/right, vertical centre/bottom for single
    // lines, clipped to the image. With dt::calcrect nothing is drawn and rect is resized to the text.
    int draw_box(RgbImage*, Face, int height, TextRect& rect, uint32_t rgb, const std::string& text,
                 unsigned flags) const;

  private:
    struct FontData;
    struct Glyph {
        int x0 = 0, y0 = 0, w = 0, h = 0, advance = 0;
        std::vector<uint8_t> ink;
    };
    const Glyph& glyph(Face, int height, uint8_t ch) const;
    const FontData& font(Face) const;
    std::map<Face, std::unique_ptr<FontData>> fonts_;
    mutable std::map<std::tuple<Face, int, uint8_t>, Glyph> cache_;
};
} // namespace rr
