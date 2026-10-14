// Plan 30 Task 2: shared check for the no-framework tests.
#pragma once
#include <iostream>
#include <string_view>
namespace rr::test {
// Records, prints "FAIL <what>", never aborts.
struct Checks {
    int failures = 0;
    bool operator()(bool ok, std::string_view what) {
        if (!ok) { ++failures; std::cerr << "FAIL " << what << '\n'; }
        return ok;
    }
    template <class A, class B> bool eq(const A& a, const B& b, std::string_view what) {
        if (a == b) return true;
        ++failures; std::cerr << "FAIL " << what << '\n';
        return false;
    }
    int finish(std::string_view name) const {
        if (failures == 0) std::cout << name << " OK\n";
        else std::cerr << name << ": " << failures << " failure(s)\n";
        return failures ? 1 : 0;
    }
};
}
