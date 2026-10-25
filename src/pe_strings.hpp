#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rr {
// RT_STRING resources of a PE DLL: string id -> UTF-16 text. Throws std::runtime_error on malformed input.
std::map<uint32_t, std::u16string> read_string_resources(const std::vector<uint8_t>& pe_dll);
// String id -> UTF-8 text.
std::map<uint32_t, std::string> read_string_table(const std::vector<uint8_t>& pe_dll);
// LoadStringA conversion to the Windows-1252 ANSI code page; unmappable characters become '?'.
std::string to_cp1252(const std::u16string&);
std::string cp1252_to_utf8(const std::string&);
char32_t cp1252_codepoint(uint8_t);

enum class Language { english, german, french, spanish, italian };
const char* language_dll(Language);                     // "TEXT/ENU.DLL" ...
Language language_from_code(const std::string& iso639); // "de" -> german; unknown -> english
Language default_language();                            // SDL preferred locales, original fallback to ENU

// Port of 0x0042f5d0: string 0 is "<buffer bytes> <count>"; strings 1..count are loaded and a
// trailing 0xff splices string 3000+<previous byte>. Result index i == original table slot i
// (the table at 0x00475378), slot 0 is empty. Strings are ANSI (Windows-1252) bytes as the game saw them.
std::vector<std::string> load_game_strings(const std::map<uint32_t, std::u16string>& resources,
                                           uint32_t base = 0);
} // namespace rr
