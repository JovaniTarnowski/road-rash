// Plan 22 Task 1 Step 5: ByteReader contract.
#include "byte_reader.hpp"
#include <iostream>
#include <limits>
#include <vector>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << "\n"; } };
    const std::vector<uint8_t> bytes{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    rr::ByteReader r({bytes.data(), bytes.size()});
    check(r.size() == 8, "size");
    check(r.le<uint16_t>(6) == 0x0807, "read at the last valid offset");
    check(r.le<uint32_t>(4) == 0x08070605u, "little-endian decode");
    check(r.le<uint8_t>(7) == 0x08, "single byte at end");
    bool threw = false;
    try { r.le<uint16_t>(7); } catch (const std::runtime_error&) { threw = true; }
    check(threw, "one byte further throws");
    threw = false;
    try { r.le<uint16_t>(SIZE_MAX); } catch (const std::runtime_error&) { threw = true; }
    check(threw, "SIZE_MAX offset throws rather than wrapping");
    threw = false;
    try { r.bytes(7, 2); } catch (const std::runtime_error&) { threw = true; }
    check(threw, "offset+count overflow throws");
    threw = false;
    try { r.bytes(8, 0); } catch (const std::runtime_error&) { threw = true; }
    check(!threw, "empty read at end succeeds");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
