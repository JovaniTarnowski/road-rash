#include "pe_strings.hpp"
#include <SDL3/SDL.h>
#include <cstdio>
#include <stdexcept>

namespace rr {
namespace {
// Windows-1252 0x80..0x9f; 0 marks the five undefined positions.
constexpr char16_t cp1252_high[32] = {0x20ac, 0,      0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
                                      0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017d, 0,
                                      0,      0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
                                      0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0,      0x017e, 0x0178};

struct Reader {
    const std::vector<uint8_t>& b;
    void range(size_t p, size_t n) const {
        if (p > b.size() || n > b.size() - p) throw std::runtime_error("Truncated PE image");
    }
    uint32_t u16(size_t p) const {
        range(p, 2);
        return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8;
    }
    uint32_t u32(size_t p) const {
        range(p, 4);
        return u16(p) | u16(p + 2) << 16;
    }
};
} // namespace

std::map<uint32_t, std::u16string> read_string_resources(const std::vector<uint8_t>& b) {
    const Reader r{b};
    if (r.u16(0) != 0x5a4d) throw std::runtime_error("Missing MZ header");
    const size_t pe = r.u32(60);
    r.range(pe, 24);
    if (r.u32(pe) != 0x4550 || r.u16(pe + 4) != 0x14c) throw std::runtime_error("Expected i386 PE image");
    const uint32_t optional_size = r.u16(pe + 20);
    if (optional_size < 96 + 3 * 8) throw std::runtime_error("PE optional header has no resource directory");
    r.range(pe + 24, optional_size);
    if (r.u16(pe + 24) != 0x10b) throw std::runtime_error("Expected PE32 optional header");
    if (r.u32(pe + 24 + 92) < 3) throw std::runtime_error("PE has no resource directory entry");
    const uint32_t resource_rva = r.u32(pe + 24 + 96 + 16);
    const size_t table = pe + 24 + optional_size;
    const uint32_t sections = r.u16(pe + 6);
    r.range(table, size_t(sections) * 40);
    // Map an RVA range to a file offset inside one section.
    auto file_offset = [&](uint32_t rva, uint32_t count) -> size_t {
        for (uint32_t i = 0; i < sections; ++i) {
            const size_t p = table + 40 * i;
            const uint32_t va = r.u32(p + 12), raw_size = r.u32(p + 16), raw = r.u32(p + 20);
            if (rva < va || rva - va >= raw_size) continue;
            if (count > raw_size - (rva - va))
                throw std::runtime_error("PE resource crosses section boundary");
            const uint64_t start = uint64_t(raw) + (rva - va);
            if (start > b.size()) throw std::runtime_error("PE section outside file");
            r.range(size_t(start), count);
            return size_t(start);
        }
        throw std::runtime_error("PE address has no file data");
    };
    std::map<uint32_t, std::u16string> out;
    if (resource_rva == 0) return out;
    const size_t base = file_offset(resource_rva, 16);
    // Resource directory: count at +12/+14, then 8-byte entries; high bit of the second word marks a subdirectory.
    auto entries = [&](uint32_t dir) {
        const size_t p = base + dir;
        r.range(p, 16);
        const uint32_t n = r.u16(p + 12) + r.u16(p + 14);
        r.range(p + 16, size_t(n) * 8);
        std::vector<std::pair<uint32_t, uint32_t>> e;
        for (uint32_t i = 0; i < n; ++i) e.emplace_back(r.u32(p + 16 + 8 * i), r.u32(p + 20 + 8 * i));
        return e;
    };
    for (const auto& [type, type_off] : entries(0)) {
        if (type != 6) continue;
        if (!(type_off & 0x80000000u)) throw std::runtime_error("RT_STRING entry is not a directory");
        for (const auto& [name, name_off] : entries(type_off & 0x7fffffffu)) {
            if (name & 0x80000000u) throw std::runtime_error("Named RT_STRING block");
            if (!(name_off & 0x80000000u) || name == 0 || name > 4096)
                throw std::runtime_error("Invalid RT_STRING block");
            for (const auto& [lang, data_off] : entries(name_off & 0x7fffffffu)) {
                (void)lang;
                if (data_off & 0x80000000u)
                    throw std::runtime_error("RT_STRING language entry is a directory");
                r.range(base + data_off, 16);
                const uint32_t rva = r.u32(base + data_off), size = r.u32(base + data_off + 4);
                size_t p = file_offset(rva, size);
                const size_t end = p + size;
                // 16 strings: uint16 length then UTF-16LE characters.
                for (uint32_t i = 0; i < 16; ++i) {
                    if (p + 2 > end) throw std::runtime_error("Truncated RT_STRING block");
                    const uint32_t length = r.u16(p);
                    p += 2;
                    if (size_t(length) * 2 > end - p) throw std::runtime_error("Truncated RT_STRING text");
                    if (length) {
                        std::u16string s(length, u'\0');
                        for (uint32_t c = 0; c < length; ++c) s[c] = char16_t(r.u16(p + 2 * c));
                        out[(name - 1) * 16 + i] = std::move(s);
                    }
                    p += size_t(length) * 2;
                }
            }
        }
    }
    return out;
}

char32_t cp1252_codepoint(uint8_t c) {
    if (c < 0x80 || c >= 0xa0) return c;
    const char16_t u = cp1252_high[c - 0x80];
    return u ? u : c; // undefined positions map to C1 controls, as MultiByteToWideChar does
}

std::string to_cp1252(const std::u16string& s) {
    std::string out;
    out.reserve(s.size());
    for (char16_t c : s) {
        if (c < 0x80 || (c >= 0xa0 && c < 0x100)) {
            out.push_back(char(c));
            continue;
        }
        char m = '?';
        for (int i = 0; i < 32; ++i)
            if (cp1252_high[i] == c) m = char(0x80 + i);
        if (c >= 0x80 && c < 0xa0) m = char(c);
        out.push_back(m);
    }
    return out;
}

std::string cp1252_to_utf8(const std::string& s) {
    std::string out;
    for (unsigned char ch : s) {
        const char32_t c = cp1252_codepoint(ch);
        if (c < 0x80)
            out.push_back(char(c));
        else if (c < 0x800) {
            out.push_back(char(0xc0 | c >> 6));
            out.push_back(char(0x80 | (c & 63)));
        } else {
            out.push_back(char(0xe0 | c >> 12));
            out.push_back(char(0x80 | (c >> 6 & 63)));
            out.push_back(char(0x80 | (c & 63)));
        }
    }
    return out;
}

std::map<uint32_t, std::string> read_string_table(const std::vector<uint8_t>& pe_dll) {
    std::map<uint32_t, std::string> out;
    for (const auto& [id, text] : read_string_resources(pe_dll)) {
        std::string u;
        for (size_t i = 0; i < text.size(); ++i) {
            char32_t c = text[i];
            if (c >= 0xd800 && c < 0xdc00 && i + 1 < text.size() && text[i + 1] >= 0xdc00 &&
                text[i + 1] < 0xe000)
                c = 0x10000 + ((c - 0xd800) << 10) + (text[++i] - 0xdc00);
            if (c < 0x80)
                u.push_back(char(c));
            else if (c < 0x800) {
                u.push_back(char(0xc0 | c >> 6));
                u.push_back(char(0x80 | (c & 63)));
            } else if (c < 0x10000) {
                u.push_back(char(0xe0 | c >> 12));
                u.push_back(char(0x80 | (c >> 6 & 63)));
                u.push_back(char(0x80 | (c & 63)));
            } else {
                u.push_back(char(0xf0 | c >> 18));
                u.push_back(char(0x80 | (c >> 12 & 63)));
                u.push_back(char(0x80 | (c >> 6 & 63)));
                u.push_back(char(0x80 | (c & 63)));
            }
        }
        out[id] = std::move(u);
    }
    return out;
}

const char* language_dll(Language l) {
    switch (l) {
    case Language::german:
        return "TEXT/DEU.DLL";
    case Language::french:
        return "TEXT/FRA.DLL";
    case Language::spanish:
        return "TEXT/ESP.DLL";
    case Language::italian:
        return "TEXT/ITA.DLL";
    default:
        return "TEXT/ENU.DLL";
    }
}

// 0x0042f340: Text\<LOCALE_SABBREVLANGNAME>.DLL, then the primary language's default sublanguage,
// then 0x409 (ENU). Only the five primary languages have DLLs on the disc.
Language language_from_code(const std::string& code) {
    const std::string c = code.substr(0, 2);
    if (c == "de") return Language::german;
    if (c == "fr") return Language::french;
    if (c == "es") return Language::spanish;
    if (c == "it") return Language::italian;
    return Language::english;
}

Language default_language() {
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    Language l = Language::english;
    if (locales && count > 0 && locales[0] && locales[0]->language)
        l = language_from_code(locales[0]->language);
    SDL_free(locales);
    return l;
}

std::vector<std::string> load_game_strings(const std::map<uint32_t, std::u16string>& resources,
                                           uint32_t base) {
    // LoadStringA: copies at most buffer-1 ANSI chars and returns how many; 0 if the id is absent.
    auto load = [&](uint32_t id, size_t buffer) -> std::string {
        const auto it = resources.find(id);
        if (it == resources.end() || buffer == 0) return {};
        std::string s = to_cp1252(it->second);
        if (s.size() > buffer - 1) s.resize(buffer - 1);
        return s;
    };
    const std::string header = load(base, 16);
    unsigned buffer = 0, count = 0;
    if (header.empty() || std::sscanf(header.c_str(), "%u %u", &buffer, &count) != 2 || count == 0 ||
        count > 65536 || buffer > (1u << 24))
        throw std::runtime_error("Language DLL has no string table header");
    std::vector<std::string> table(count + 1);
    size_t remaining = buffer;
    for (uint32_t id = count; id != 0; --id) {
        std::string s = load(base + id, remaining);
        if (s.empty()) continue;
        size_t used = s.size();
        // A trailing 0xff splices string 3000 + (signed) the byte before it over those two bytes.
        for (int guard = 0; uint8_t(s.back()) == 0xff; ++guard) {
            if (s.size() < 2 || guard > 64) throw std::runtime_error("Invalid string continuation");
            const int index = int8_t(s[s.size() - 2]);
            s.resize(s.size() - 2);
            remaining += 2 - used;
            std::string next = load(uint32_t(3000 + index), remaining);
            if (next.empty()) throw std::runtime_error("Missing string continuation");
            s += next;
            used = next.size();
        }
        if (used + 1 > remaining) throw std::runtime_error("String table buffer overflow");
        remaining -= used + 1;
        table[id] = std::move(s);
    }
    return table;
}
} // namespace rr
