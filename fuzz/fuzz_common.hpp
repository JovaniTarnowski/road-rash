#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>
// Runs a parser under the project contract: return or std::runtime_error. Any other exception aborts
// (libFuzzer records the input as a crash). Sanitizers, timeouts and RSS limits catch the rest.
template <class F> int rr_fuzz_contract(const uint8_t* data, size_t size, F&& parse) {
    try { parse(std::vector<uint8_t>(data, data + size)); }
    catch (const std::runtime_error&) {}
    catch (const std::exception& e) { std::fprintf(stderr, "contract: unexpected %s\n", e.what()); std::abort(); }
    return 0;
}
