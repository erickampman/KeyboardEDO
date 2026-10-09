#pragma once

// The same minimal test support as uw-core's tests/Check.h: each test file is
// its own executable, CHECK() records failures, main() returns finish("suite").

#include <cmath>
#include <cstdio>

namespace kedo::test {

inline int failures = 0;

inline int finish(const char* suite) {
    if (failures != 0) {
        std::fprintf(stderr, "%s: %d check(s) failed\n", suite, failures);
        return 1;
    }
    std::printf("%s tests passed\n", suite);
    return 0;
}

inline bool near(double a, double b, double eps = 1e-6) { return std::abs(a - b) <= eps; }

} // namespace kedo::test

#define CHECK(expr)                                                                \
    do {                                                                           \
        if (!(expr)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,  \
                         #expr);                                                   \
            ++kedo::test::failures;                                                \
        }                                                                          \
    } while (0)
