#include <string>

#include "settings.hpp"

// parse_settings must never throw: unknown keys and malformed values are
// ignored. No contract wrapper, so any throw becomes a fuzzer crash.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string text = size ? std::string(reinterpret_cast<const char*>(data), size) : std::string();
    (void)rr::parse_settings(text);
    return 0;
}
