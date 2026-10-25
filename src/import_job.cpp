// Pull-based import job (plan 23 Task 4): answers metadata and file data
// through byte-range / whole-file needs so sync (native) and async (browser)
// hosts drive the same verification logic. ISO metadata rules mirror
// iso9660.cpp (descriptor chain, dual-endian fields, record bounds,
// uppercase compare with ';' version strip, directory cycle guard); table
// rules mirror import_game in importer.cpp.
//
// Needs are computed lazily: observing the job (need_*(), progress()) may
// store the offered need, so the lazily-advanced members are mutable. All
// real state transitions happen in supply()/missing_file()/cancel().
#include "import_job.hpp"
#include "byte_reader.hpp"
#include "importer.hpp"
#include "iso9660.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>

namespace rr {
namespace {
constexpr uint32_t kChunk = 1u << 20; // file data requests are at most 1 MiB
constexpr uint64_t kMaxFile = 256ull * 1024 * 1024;
constexpr uint64_t kMaxDir = 16ull * 1024 * 1024;

uint32_t both32(const ByteReader& r, size_t off) {
    const uint32_t value = r.le<uint32_t>(off);
    const uint32_t other = uint32_t(r.bytes(off + 4, 1)[0]) << 24 | uint32_t(r.bytes(off + 5, 1)[0]) << 16 |
                           uint32_t(r.bytes(off + 6, 1)[0]) << 8 | uint32_t(r.bytes(off + 7, 1)[0]);
    if (value != other) throw std::runtime_error("ISO inconsistent dual-endian field");
    return value;
}
void validate_table(const std::vector<ReferenceHash>& hashes) {
    if (hashes.empty()) throw std::runtime_error("Empty import reference table");
    std::set<std::string> paths;
    for (const auto& entry : hashes) {
        const std::string& path = entry.path;
        if (path.empty() || path.find('\\') != std::string::npos || path.find('\0') != std::string::npos ||
            path == ".rr-import" || path == ".rr-source.iso")
            throw std::runtime_error("Invalid import reference path");
        size_t at = 0;
        while (at < path.size()) {
            const size_t end = path.find('/', at);
            const std::string part = path.substr(at, end == std::string::npos ? end : end - at);
            if (part.empty() || part == "." || part == "..")
                throw std::runtime_error("Import reference escapes destination");
            if (end == std::string::npos) break;
            at = end + 1;
        }
        if (!paths.insert(path).second || entry.hash.size() != 64 ||
            entry.hash.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::runtime_error("Invalid import reference table");
    }
}
std::vector<std::string> split_iso_path(const std::string& full) {
    std::vector<std::string> parts;
    size_t at = 0;
    while (at < full.size()) {
        const size_t end = full.find('/', at);
        std::string name = full.substr(at, end == std::string::npos ? end : end - at);
        if (name.empty() || name == "." || name == ".." || name.find('\\') != std::string::npos)
            throw std::runtime_error("Invalid ISO path");
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return char(std::toupper(c)); });
        parts.push_back(name);
        if (end == std::string::npos) break;
        at = end + 1;
        if (at == full.size()) throw std::runtime_error("Invalid trailing ISO slash");
    }
    return parts;
}
} // namespace

struct ImportJob::Impl {
    std::vector<ReferenceHash> hashes;
    ImportSink* sink = nullptr;
    bool iso = true;
    ImportResult result;
    bool finished = false;
    size_t files_done = 0;

    enum class Phase { descriptors, dirs, files, final };
    Phase phase = Phase::descriptors;

    // Lazily offered need (mutable: observing the job stores the offer).
    // supply() consumes it; a second supply() without a fresh need throws.
    enum class Need { none, descriptor, volume_tail, dir, file_chunk, file_whole };
    mutable Need pending = Need::none;
    mutable ByteRange pending_range{};
    mutable uint32_t pending_dir_sector = 0, pending_dir_size = 0;
    mutable std::string pending_file;
    mutable std::string current;

    // ISO metadata.
    uint64_t descriptor_sector = 16;
    bool pvd_found = false;
    uint64_t volume_size = 0;
    uint32_t root_sector = 0, root_size = 0;
    std::map<uint64_t, std::vector<uint8_t>> dir_cache; // extent offset -> bytes
    struct Extent {
        uint32_t sector = 0, size = 0;
        bool missing = false;
        std::string why;
    };
    std::vector<Extent> extents;

    // Current file transfer.
    size_t file_index = 0;
    std::vector<uint8_t> file_buf; // data chunks until the hash verifies
    Sha256Stream hasher;
    bool hashing = false;
    uint32_t file_size = 0, file_got = 0;

    struct Record {
        std::string name;
        uint32_t sector = 0, size = 0;
        bool directory = false;
    };
    Record parse_record(const uint8_t* p, size_t available) const {
        // Shared decoder (iso9660.hpp); extent checks stay here.
        const IsoRecord r = parse_iso_record({p, available});
        Record e{r.name, r.sector, r.size, r.directory};
        const uint64_t offset = uint64_t(e.sector) * 2048;
        if (offset > volume_size || e.size > volume_size - offset)
            throw std::runtime_error("ISO extent outside volume");
        return e;
    }

    std::vector<Record> list_dir(uint32_t sector, uint32_t size) const {
        const uint64_t offset = uint64_t(sector) * 2048;
        auto it = dir_cache.find(offset);
        if (it == dir_cache.end() || it->second.size() != size)
            throw std::runtime_error("ISO directory not loaded");
        const auto& data = it->second;
        std::vector<Record> result;
        std::set<std::string> names;
        for (size_t at = 0; at < data.size();) {
            if (!data[at]) {
                at = (at / 2048 + 1) * 2048;
                continue;
            }
            if (data[at] > 2048 - at % 2048) throw std::runtime_error("ISO record crosses sector");
            auto e = parse_record(data.data() + at, data.size() - at);
            at += data[at];
            if (e.name == std::string(1, '\0') || e.name == std::string(1, '\1')) continue;
            if (e.name.empty() || e.name == "." || e.name == ".." ||
                e.name.find_first_of("/\\") != std::string::npos)
                throw std::runtime_error("Invalid ISO entry name");
            if (!names.insert(e.name).second) throw std::runtime_error("Duplicate ISO name");
            result.push_back(std::move(e));
        }
        return result;
    }

    bool dir_cached(uint32_t sector, uint32_t size) const {
        const uint64_t offset = uint64_t(sector) * 2048;
        auto it = dir_cache.find(offset);
        return it != dir_cache.end() && it->second.size() == size;
    }

    void fail(const std::string& message) {
        if (finished) return;
        result.error = message;
        result.ok = false;
        pending = Need::none;
        sink->abort();
        finished = true;
    }

    // Walk one ROADRASH/ path using cached directories. Fills out and
    // returns true when fully resolved; returns false with blocked_* set
    // when the walk needs an uncached directory; throws the missing/invalid
    // reason otherwise (messages mirror Iso9660::locate).
    bool walk(const std::vector<std::string>& parts, const std::string& full, Extent& out,
              uint32_t& blocked_sector, uint32_t& blocked_size) const {
        uint32_t sector = root_sector, size = root_size;
        std::set<uint32_t> visited{root_sector};
        for (size_t i = 0; i < parts.size(); ++i) {
            if (!dir_cached(sector, size)) {
                blocked_sector = sector;
                blocked_size = size;
                return false;
            }
            const auto children = list_dir(sector, size);
            const auto child = std::find_if(children.begin(), children.end(),
                                            [&](const Record& e) { return e.name == parts[i]; });
            if (child == children.end()) throw std::runtime_error("Missing ISO path: " + full);
            const bool last = i + 1 == parts.size();
            if (child->directory) {
                if (last) throw std::runtime_error("Cannot read ISO directory as file");
                if (!visited.insert(child->sector).second) throw std::runtime_error("ISO directory cycle");
                if (child->size > kMaxDir) throw std::runtime_error("ISO extent too large");
                sector = child->sector;
                size = child->size;
            } else {
                if (!last) throw std::runtime_error("ISO path is not a directory");
                if (child->size > kMaxFile) throw std::runtime_error("ISO extent too large");
                out.sector = child->sector;
                out.size = child->size;
            }
        }
        return true;
    }

    // First uncached directory in reference order. Returns true with
    // sector/size set, or false when every reachable directory is cached
    // (paths missing for other reasons are left for resolve_all to record).
    bool scan_dirs(uint32_t& sector, uint32_t& size) const {
        for (const auto& entry : hashes) {
            const std::string full = "ROADRASH/" + entry.path;
            std::vector<std::string> parts;
            try {
                parts = split_iso_path(full);
            } catch (const std::exception&) {
                continue;
            }
            Extent ignored;
            try {
                if (!walk(parts, full, ignored, sector, size)) return true;
            } catch (const std::exception&) {
                continue;
            }
        }
        return false;
    }

    void resolve_all() {
        extents.clear();
        for (const auto& entry : hashes) {
            const std::string full = "ROADRASH/" + entry.path;
            Extent found;
            try {
                const auto parts = split_iso_path(full);
                uint32_t sector = 0, size = 0;
                if (!walk(parts, full, found, sector, size))
                    throw std::runtime_error("ISO directory not loaded");
            } catch (const std::exception& e) {
                found.missing = true;
                found.why = e.what();
            }
            extents.push_back(found);
        }
    }

    // Start fetching file_index (ISO extents resolved). Cascades through
    // missing and empty files; leaves a real need pending or finalizes.
    void start_file() {
        for (;;) {
            if (file_index >= hashes.size()) {
                finalize();
                return;
            }
            const std::string& path = hashes[file_index].path;
            current = path;
            if (!iso) return; // need_file() offers the whole file
            const Extent& e = extents[file_index];
            if (e.missing) {
                result.missing.push_back(path + ": " + e.why);
                ++files_done;
                ++file_index;
                continue;
            }
            file_size = e.size;
            file_got = 0;
            file_buf.clear();
            hasher = Sha256Stream();
            hashing = true;
            if (file_size == 0) {
                finish_file_data();
                continue;
            }
            return; // need_range() offers the first chunk
        }
    }

    // The current file's bytes are complete in file_buf: verify and deliver.
    void finish_file_data() {
        const std::string& path = hashes[file_index].path;
        const std::string digest = hasher.finish();
        hashing = false;
        if (digest != hashes[file_index].hash) {
            result.mismatched.push_back(path);
        } else {
            sink->begin_file(path, file_buf.size());
            if (!file_buf.empty()) sink->write(file_buf.data(), file_buf.size());
            sink->end_file(true);
        }
        file_buf.clear();
        file_buf.shrink_to_fit();
        ++files_done;
        ++file_index;
    }

    void finalize() {
        phase = Phase::final;
        pending = Need::none;
        if (!result.error.empty() || !result.missing.empty() || !result.mismatched.empty()) {
            result.ok = false;
            sink->abort();
            finished = true;
            return;
        }
        try {
            result.ok = sink->commit();
            if (!result.ok) result.error = "import sink refused commit";
        } catch (const std::exception& e) {
            result.ok = false;
            result.error = e.what();
        }
        if (!result.ok) {
            try {
                sink->abort();
            } catch (...) {
            }
        }
        finished = true;
    }

    void got_descriptor(const std::vector<uint8_t>& sector) {
        if (std::memcmp(sector.data() + 1, "CD001", 5) || sector[6] != 1)
            throw std::runtime_error("Not an ISO 9660 image");
        if (sector[0] == 255) {
            if (!pvd_found) throw std::runtime_error("ISO primary volume descriptor missing");
            phase = Phase::dirs;
            return;
        }
        if (sector[0] != 1) {
            ++descriptor_sector;
            if (descriptor_sector >= 64) throw std::runtime_error("ISO primary volume descriptor missing");
            return;
        }
        if (sector[128] != 0 || sector[129] != 8 || sector[130] != 8 || sector[131] != 0)
            throw std::runtime_error("ISO block size must be 2048");
        volume_size = uint64_t(both32(ByteReader(sector), 80)) * 2048;
        if (!volume_size) throw std::runtime_error("ISO declared volume is truncated");
        const Record root = parse_record(sector.data() + 156, sector.size() - 156);
        if (!root.directory) throw std::runtime_error("ISO root is not a directory");
        if (root.size > kMaxDir) throw std::runtime_error("ISO extent too large");
        root_sector = root.sector;
        root_size = root.size;
        pvd_found = true;
    }
};

ImportJob::ImportJob(std::vector<ReferenceHash> hashes, ImportSink& sink, bool iso) : impl_(new Impl()) {
    Impl& m = *impl_;
    try {
        validate_table(hashes);
    } catch (const std::exception& e) {
        m.result.error = e.what();
        m.result.ok = false;
        m.finished = true;
        return;
    }
    m.hashes = std::move(hashes);
    m.sink = &sink;
    m.iso = iso;
    if (!iso) {
        m.phase = Impl::Phase::files;
        m.current = m.hashes.front().path;
    }
}

ImportJob::~ImportJob() = default;

std::optional<ByteRange> ImportJob::need_range() const {
    const Impl& m = *impl_;
    if (m.finished || !m.iso) return std::nullopt;
    if (m.pending != Impl::Need::none) {
        if (m.pending == Impl::Need::descriptor || m.pending == Impl::Need::volume_tail ||
            m.pending == Impl::Need::dir || m.pending == Impl::Need::file_chunk)
            return m.pending_range;
        return std::nullopt;
    }
    if (m.phase == Impl::Phase::descriptors) {
        if (!m.pvd_found) {
            m.pending = Impl::Need::descriptor;
            m.pending_range = {m.descriptor_sector * 2048, 2048};
            return m.pending_range;
        }
        m.pending = Impl::Need::volume_tail;
        m.pending_range = {m.volume_size - 1, 1};
        return m.pending_range;
    }
    if (m.phase == Impl::Phase::dirs) {
        uint32_t sector = 0, size = 0;
        if (m.scan_dirs(sector, size)) {
            m.pending = Impl::Need::dir;
            m.pending_dir_sector = sector;
            m.pending_dir_size = size;
            m.pending_range = {uint64_t(sector) * 2048, size};
            m.current = "Reading disc image";
            return m.pending_range;
        }
        // Unreachable in correct driving: the last dir supply() resolves and
        // moves to files. Returning nothing would stall the host, so fall
        // through to the files branch below is impossible here (phase is
        // still dirs); the host must have already supplied everything.
        return std::nullopt;
    }
    if (m.phase == Impl::Phase::files) {
        if (m.file_index >= m.hashes.size() || m.file_index >= m.extents.size()) return std::nullopt;
        const Impl::Extent& e = m.extents[m.file_index];
        if (e.missing || m.file_got >= m.file_size) return std::nullopt; // consumed by the start_file cascade
        m.pending = Impl::Need::file_chunk;
        const uint32_t left = m.file_size - m.file_got;
        m.pending_range = {uint64_t(e.sector) * 2048 + m.file_got, std::min(left, kChunk)};
        m.current = m.hashes[m.file_index].path;
        return m.pending_range;
    }
    return std::nullopt;
}

std::optional<std::string> ImportJob::need_file() const {
    const Impl& m = *impl_;
    if (m.finished || m.iso) return std::nullopt;
    if (m.pending != Impl::Need::none) {
        if (m.pending == Impl::Need::file_whole) return m.pending_file;
        return std::nullopt;
    }
    if (m.phase == Impl::Phase::files && m.file_index < m.hashes.size()) {
        m.pending = Impl::Need::file_whole;
        m.pending_file = m.hashes[m.file_index].path;
        m.current = m.pending_file;
        return m.pending_file;
    }
    return std::nullopt;
}

void ImportJob::supply(const uint8_t* data, size_t size) {
    Impl& m = *impl_;
    if (m.finished) throw std::runtime_error("supply after the job finished");
    if (m.pending == Impl::Need::none) throw std::runtime_error("supply with no pending need");
    const Impl::Need was = m.pending;
    m.pending = Impl::Need::none;
    try {
        if (was == Impl::Need::file_whole) {
            if (m.phase != Impl::Phase::files || m.iso)
                throw std::runtime_error("supply with no pending need");
            const std::string& path = m.hashes[m.file_index].path;
            Sha256Stream hasher;
            if (size) hasher.write(data, size);
            if (hasher.finish() != m.hashes[m.file_index].hash) {
                m.result.mismatched.push_back(path);
            } else {
                std::vector<uint8_t> bytes(size);
                if (size) std::memcpy(bytes.data(), data, size);
                m.sink->begin_file(path, bytes.size());
                if (!bytes.empty()) m.sink->write(bytes.data(), bytes.size());
                m.sink->end_file(true);
            }
            ++m.files_done;
            ++m.file_index;
            if (m.file_index >= m.hashes.size()) {
                m.finalize();
                return;
            }
            m.current = m.hashes[m.file_index].path;
            return;
        }
        // ISO range needs carry an exact size; a short answer means the
        // source is truncated (messages mirror the Iso9660 constructor/load).
        const uint32_t want = m.pending_range.size;
        if (size != want) {
            m.fail(was == Impl::Need::descriptor    ? "Truncated ISO descriptor"
                   : was == Impl::Need::volume_tail ? "ISO declared volume is truncated"
                                                    : "Truncated ISO extent");
            return;
        }
        std::vector<uint8_t> bytes(size);
        if (size) std::memcpy(bytes.data(), data, size);
        if (was == Impl::Need::descriptor) {
            m.got_descriptor(bytes);
            return;
        }
        if (was == Impl::Need::volume_tail) {
            m.phase = Impl::Phase::dirs;
            return;
        }
        if (was == Impl::Need::dir) {
            m.dir_cache[uint64_t(m.pending_dir_sector) * 2048] = std::move(bytes);
            uint32_t sector = 0, size = 0;
            if (!m.scan_dirs(sector, size)) {
                m.resolve_all();
                m.phase = Impl::Phase::files;
                m.file_index = 0;
                m.start_file();
            }
            return;
        }
        if (was == Impl::Need::file_chunk) {
            if (!m.hashing) throw std::runtime_error("supply with no pending need");
            m.hasher.write(bytes.data(), bytes.size());
            m.file_buf.insert(m.file_buf.end(), bytes.begin(), bytes.end());
            m.file_got += want;
            if (m.file_got == m.file_size) {
                m.finish_file_data();
                m.start_file();
            }
            return;
        }
    } catch (const std::exception& e) {
        m.fail(e.what());
        return;
    }
    throw std::runtime_error("supply with no pending need");
}

void ImportJob::missing_file(const std::string& message) {
    Impl& m = *impl_;
    if (m.finished) throw std::runtime_error("missing_file after the job finished");
    if (m.pending != Impl::Need::file_whole || m.iso)
        throw std::runtime_error("missing_file with no pending file need");
    m.pending = Impl::Need::none;
    const std::string& path = m.hashes[m.file_index].path;
    m.result.missing.push_back(path + ": " + message);
    ++m.files_done;
    ++m.file_index;
    if (m.file_index >= m.hashes.size()) {
        m.finalize();
        return;
    }
    m.current = m.hashes[m.file_index].path;
}

void ImportJob::fail_source(const std::string& message) {
    Impl& m = *impl_;
    m.fail(message);
}

bool ImportJob::done() const {
    return impl_->finished;
}

const ImportResult& ImportJob::result() const {
    return impl_->result;
}

ImportProgress ImportJob::progress() const {
    const Impl& m = *impl_;
    return {m.files_done, m.hashes.size(), m.current};
}

void ImportJob::cancel() {
    Impl& m = *impl_;
    if (m.finished) return;
    m.result.error = "canceled";
    m.result.ok = false;
    m.pending = Impl::Need::none;
    m.sink->abort();
    m.finished = true;
}

} // namespace rr
