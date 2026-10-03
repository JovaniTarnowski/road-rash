#include "sha256.hpp"
#include <iostream>
int main() {
    int failures=0;
    auto check=[&](const std::string& input,const std::string& expected) {
        if (rr::sha256(std::vector<uint8_t>(input.begin(),input.end()))!=expected) { ++failures; std::cerr << "FAIL SHA-256 vector length " << input.size() << '\n'; }
    };
    check("","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check("abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq","248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    check(std::string(1000000,'a'),"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
