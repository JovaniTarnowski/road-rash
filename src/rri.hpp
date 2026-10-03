#pragma once
#include "text.hpp"
#include <string>

namespace rr {
class Assets;
// Menu pictures (IMAGES/*.RRI, JPEG). The original converts them to a 15-bit surface
// (0x0041e430 / 0x0041e6b0); pixels are quantised to RGB555 here.
RgbImage decode_rri(const std::vector<uint8_t>& bytes);
// "Postcard/City" -> IMAGES/POSTCARD/CITY.RRI. Throws when missing or undecodable.
RgbImage load_menu_image(const Assets&, const std::string& name);
bool menu_image_exists(const Assets&, const std::string& name);
// 0x0041e6e0: the second surface used behind text boxes, each 5-bit channel halved.
uint32_t darken555(uint32_t rgb);
uint32_t quantise555(uint32_t rgb);
}
