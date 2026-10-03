#include "rri.hpp"
#include "assets.hpp"
#include <cctype>
#include <stdexcept>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include "stb_image.h"

namespace rr {
uint32_t quantise555(uint32_t c) {
    auto q = [](uint32_t v) { v >>= 3; return (v << 3) | (v >> 2); };
    return q(c >> 16 & 0xff) << 16 | q(c >> 8 & 0xff) << 8 | q(c & 0xff);
}

uint32_t darken555(uint32_t c) {
    auto d = [](uint32_t v) { v = (v >> 3) >> 1; return (v << 3) | (v >> 2); };
    return d(c >> 16 & 0xff) << 16 | d(c >> 8 & 0xff) << 8 | d(c & 0xff);
}

RgbImage decode_rri(const std::vector<uint8_t>& bytes) {
    int w = 0, h = 0, n = 0;
    if (bytes.size() > 0x7fffffff) throw std::runtime_error("RRI image too large");
    stbi_uc* p = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &n, 3);
    if (!p) throw std::runtime_error("Invalid RRI image");
    RgbImage img(w, h);
    for (size_t i = 0; i < img.pixels.size(); ++i)
        img.pixels[i] = quantise555(uint32_t(p[3 * i]) << 16 | uint32_t(p[3 * i + 1]) << 8 | p[3 * i + 2]);
    stbi_image_free(p);
    return img;
}

static std::string image_path(const std::string& name) {
    std::string p = "IMAGES/";
    for (char c : name) p.push_back(c == '\\' ? '/' : char(std::toupper(static_cast<unsigned char>(c))));
    return p + ".RRI";
}

bool menu_image_exists(const Assets& a, const std::string& name) { return a.exists(image_path(name)); }

RgbImage load_menu_image(const Assets& a, const std::string& name) { return decode_rri(a.read(image_path(name))); }
}
