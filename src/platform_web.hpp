#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace rr {
std::vector<uint8_t> web_read_asset(const std::string& path);
bool web_asset_exists(const std::string& path);
} // namespace rr
