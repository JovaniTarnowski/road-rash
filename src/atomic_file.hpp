#pragma once
#include <cstddef>
#include <filesystem>

namespace rr {
// Writes `path` via `path + ".tmp"`, flush + fsync, then rename over the
// target. Returns false and leaves any existing file untouched on failure.
bool write_file_atomic(const std::filesystem::path& path, const void* data, size_t size);
} // namespace rr
