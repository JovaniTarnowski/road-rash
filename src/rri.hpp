#pragma once
#include "text.hpp"
#include <string>

namespace rr {
class Assets;
// Menu pictures (IMAGES/*.RRI, JPEG). The original converts them through the
// 555->display table built by 0x0041e090 (0x0041e430 / 0x0041e6b0); captured
// reference tables map 555 inputs to RGB565, so pixels are quantised here.
RgbImage decode_rri(const std::vector<uint8_t>& bytes);
// "Postcard/City" -> IMAGES/POSTCARD/CITY.RRI. Throws when missing or undecodable.
RgbImage load_menu_image(const Assets&, const std::string& name);
bool menu_image_exists(const Assets&, const std::string& name);
// 0x0041e6e0: the second surface used behind text boxes, each 5-bit channel halved.
// Measured against the original: dark fills equal halved-555 of the decoded
// photo (button bar, description boxes), not the 0x7bde-masked 565 mapping;
// the picture surface itself is 565 (0x0041e090 table proof).
uint32_t darken555(uint32_t rgb);
uint32_t quantise565(uint32_t rgb);
}
