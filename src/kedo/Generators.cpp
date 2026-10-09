#include "kedo/Generators.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace kedo {

int gcd(int a, int b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0) {
        const int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

std::vector<Generator> generatorPresets(int edo) {
    auto steps = [edo](double ratio) { return int(std::lround(edo * std::log2(ratio))); };
    const int p5 = steps(3.0 / 2.0);
    const int p4 = edo - p5;
    const int m2 = steps(9.0 / 8.0);
    const int maj3 = steps(5.0 / 4.0);
    const int min3 = steps(6.0 / 5.0);

    std::vector<Generator> presets;

    // Wicki-Hayden: right = major second, up-right = fifth (so up-left = fourth).
    if (m2 > 0 && p5 > m2 && gcd(m2, p5) == 1)
        presets.push_back({"Wicki-Hayden", m2, p5});

    // Bosanquet: right = one step, up-right = fifth.
    if (p5 > 1)
        presets.push_back({"Bosanquet", 1, p5});

    // Janko: whole tones along a row, the next row up a step higher.
    if (m2 > 1)
        presets.push_back({"Janko", m2, 1});

    // Harmonic Table: up-right = major third, up-left = minor third (so two
    // rows up is a fifth, and right is their difference).
    if (maj3 > min3 && min3 > 0 && gcd(maj3 - min3, maj3) == 1)
        presets.push_back({"Harmonic Table", maj3 - min3, maj3});

    // Guitar (fourths): right = major second, up-right = fourth.
    if (m2 > 0 && p4 > 1 && p4 != p5 && gcd(m2, p4) == 1)
        presets.push_back({"Guitar (fourths)", m2, p4});

    if (presets.empty())
        presets.push_back({"Default", 1, std::max(2, int(std::lround(edo * 0.585)))});

    return presets;
}

} // namespace kedo
