#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <functional>
#include <optional>
#include <string>
#include <vector>
namespace rr {
enum class UpscaleMethod : uint8_t { ai = 0, mmpx = 1 };
constexpr uint32_t mmpx_version = 1; // bump when mmpx.cpp output changes
struct UpscaledImage {
    int width = 0, height = 0;
    bool indexed = true; // true: 1 byte/px palette indices; false: 0x00RRGGBB/px
    std::vector<uint8_t> indices;
    std::vector<uint32_t> rgb;
};
// Local upscale cache (plan 20 Task 1): generated on the user's machine from
// the user's imported disc into <pref>/upscaled. Never in git, the source
// tree, fixtures, the APK or a release archive. A stale or missing entry
// falls back to the next method, finally to the original image; a broken
// cache can never stop the game.
// Raw SHA-256 of source bytes (matches the recorded entry hash).
std::array<uint8_t, 32> hash_source32(const std::vector<uint8_t>& source);
class UpscaleCache {
public:
    explicit UpscaleCache(std::string root); // <pref>/upscaled
    // Best entry for (asset, sub-image index, scale) whose recorded source
    // hash equals sha256(source) and whose method version is current, trying
    // `methods` in order. Malformed files are treated as missing, never
    // thrown to the game. Throws std::runtime_error only on bad arguments.
    std::optional<UpscaledImage> find(const std::string& asset, uint32_t index, int scale,
        const std::vector<uint8_t>& source,
        std::initializer_list<UpscaleMethod> methods) const;
    // Content-addressed lookup (plan 20 Task 3): first entry under root whose
    // recorded source hash equals sha256(source), any asset/index. Lets the
    // scene attach without tracking the generator's enumeration identity.
    std::optional<UpscaledImage> find_by_content(const std::vector<uint8_t>& source, int scale,
        std::initializer_list<UpscaleMethod> methods) const;
    // Batch index (plan 20 Task 3): hash -> image for every entry matching
    // (scale, methods). One walk serves a whole scene attach.
    std::map<std::pair<std::array<uint8_t, 32>, int>, UpscaledImage> content_index(
        std::initializer_list<int> scales, std::initializer_list<UpscaleMethod> methods) const;
    // Writes <root>/<method>/<scale>/<asset>/<index>.rrup via a temp file and
    // rename. Returns false (never throws) on I/O failure.
    bool store(const std::string& asset, uint32_t index, int scale,
        const std::vector<uint8_t>& source, UpscaleMethod, const UpscaledImage&) const;
    struct PackResult { int imported = 0, skipped = 0; };
    // Imports a synthetic pack zip (plan 20 Task 5): returns {imported,
    // skipped}. Hash mismatches and truncated entries are skipped, never
    // thrown. `expected_source` supplies the source bytes per (asset,index)
    // for hash validation. Throws std::runtime_error on a malformed zip.
    PackResult import_pack(const std::vector<uint8_t>& zip,
        const std::function<std::vector<uint8_t>(const std::string&, uint32_t)>& expected_source) const;
    const std::string& root() const { return root_; }
private:
    std::string root_;
};
// `.rrup` format: magic `RRUP`, u32 version 1, u8 method, u32 method version,
// 32-byte source SHA-256 (raw bytes), u32 width, u32 height, u8 indexed, then
// pixels. All reads bounds-checked. Exported for tests and import_pack.
std::vector<uint8_t> encode_rrup(UpscaleMethod, uint32_t method_version,
    const std::array<uint8_t,32>& source_hash, const UpscaledImage&);
// Returns nullopt for any malformation or version/method mismatch the caller
// does not accept (pass 0 to accept any method_version, e.g. to test stale
// detection by comparison). Never throws on content; throws
// std::runtime_error on bad out-parameters only.
std::optional<UpscaledImage> decode_rrup(const std::vector<uint8_t>& bytes,
    UpscaleMethod* method_out, uint32_t* method_version_out,
    std::array<uint8_t,32>* source_hash_out);
// Raw 32-byte SHA-256 of source bytes (cache keys use content hash plus
// resource/subframe/crop identity, method/version, palette/remap policy,
// alpha policy and scale).
std::array<uint8_t,32> source_hash32(const std::vector<uint8_t>& source);
// ZIP validation shared with fuzz work (plan 22): rejects path traversal,
// duplicate names, symlinks and excessive inflated sizes before allocation.
// Returns the (name, data) entries; throws std::runtime_error.
std::vector<std::pair<std::string,std::vector<uint8_t>>> unzip_checked(
    const std::vector<uint8_t>& zip, size_t max_total = 64u*1024u*1024);
}
