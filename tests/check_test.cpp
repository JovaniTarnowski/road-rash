// Plan 30 Task 2: shared Checks helper test.
#include "check.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << '\n';
        }
    };
    {
        rr::test::Checks c;
        check(c(true, "pass records nothing") && c.failures == 0, "pass path");
        check(!c(false, "fail records") && c.failures == 1, "fail path returns false");
        check(c.eq(1, 1, "int eq") && c.failures == 1, "eq pass");
        check(!c.eq(1, 2, "int neq") && c.failures == 2, "eq fail");
        check(!c.eq(std::string("a"), std::string("b"), "string neq") && c.failures == 3, "eq strings");
        check(c.finish("checks") == 1, "finish returns 1 with failures");
    }
    {
        rr::test::Checks c;
        c(true, "x");
        check(c.finish("checks") == 0, "finish returns 0 when clean");
    }
    if (failures == 0) std::cout << "check OK\n";
    return failures ? 1 : 0;
}
