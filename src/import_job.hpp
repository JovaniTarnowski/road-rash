#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rr {
struct ReferenceHash;
struct ImportProgress;
struct ImportResult;

// Pull-based import (plan 23 Task 4): the job asks for what it needs next
// and the host feeds it. The synchronous native driver answers ranges/files
// from SDL_IOStream reads; the browser answers ISO ranges with
// File.slice().arrayBuffer() and folder files with directory-picker reads.
// Nothing is committed unless every hash matches, mirroring import_game.
struct ByteRange {
    uint64_t offset = 0;
    uint32_t size = 0;
};

// Where verified files go: native writes to the staging directory; web
// writes to browser storage.
class ImportSink {
  public:
    virtual ~ImportSink() = default;
    virtual void begin_file(const std::string& path, uint64_t size) = 0;
    virtual void write(const uint8_t*, size_t) = 0;
    virtual void end_file(bool ok) = 0;
    virtual bool commit() = 0;
    virtual void abort() = 0;
};

class ImportJob {
  public:
    // source_kind: an ISO image, or a folder whose files are read by path.
    ImportJob(std::vector<ReferenceHash> hashes, ImportSink& sink, bool iso);
    ~ImportJob();
    ImportJob(const ImportJob&) = delete;
    ImportJob& operator=(const ImportJob&) = delete;
    ImportJob(ImportJob&&) = default;
    ImportJob& operator=(ImportJob&&) = default;
    // What the job needs next: a byte range of the ISO, or a whole file by
    // path (folder imports).
    std::optional<ByteRange> need_range() const;
    std::optional<std::string> need_file() const;
    void supply(const uint8_t* data, size_t size); // answer to the current need
    void
    missing_file(const std::string& message);     // folder file absent/unreadable: records missing, continues
    void fail_source(const std::string& message); // the host could not read
    bool done() const;
    const ImportResult& result() const;
    ImportProgress progress() const;
    void cancel();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rr
