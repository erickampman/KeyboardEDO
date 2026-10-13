#include "kedo/ComputerKeys.h"

#include <algorithm>

namespace kedo {

namespace {

// HID usages (USB HID Usage Tables, Keyboard page) and their W3C codes.
struct Named {
    int key;
    const char* code;
    const char* name;
};

const Named kNamed[] = {
    {4, "KeyA", "A"},   {5, "KeyB", "B"},   {6, "KeyC", "C"},   {7, "KeyD", "D"},   {8, "KeyE", "E"},
    {9, "KeyF", "F"},   {10, "KeyG", "G"},  {11, "KeyH", "H"},  {12, "KeyI", "I"},  {13, "KeyJ", "J"},
    {14, "KeyK", "K"},  {15, "KeyL", "L"},  {16, "KeyM", "M"},  {17, "KeyN", "N"},  {18, "KeyO", "O"},
    {19, "KeyP", "P"},  {20, "KeyQ", "Q"},  {21, "KeyR", "R"},  {22, "KeyS", "S"},  {23, "KeyT", "T"},
    {24, "KeyU", "U"},  {25, "KeyV", "V"},  {26, "KeyW", "W"},  {27, "KeyX", "X"},  {28, "KeyY", "Y"},
    {29, "KeyZ", "Z"},  {30, "Digit1", "1"}, {31, "Digit2", "2"}, {32, "Digit3", "3"}, {33, "Digit4", "4"},
    {34, "Digit5", "5"}, {35, "Digit6", "6"}, {36, "Digit7", "7"}, {37, "Digit8", "8"}, {38, "Digit9", "9"},
    {39, "Digit0", "0"}, {45, "Minus", "-"}, {46, "Equal", "="}, {47, "BracketLeft", "["},
    {48, "BracketRight", "]"}, {51, "Semicolon", ";"}, {52, "Quote", "'"}, {54, "Comma", ","},
    {55, "Period", "."}, {56, "Slash", "/"},
};

const Named* named(int key) {
    for (const Named& n : kNamed)
        if (n.key == key) return &n;
    return nullptr;
}

} // namespace

const std::vector<int>& computerKeyRow(int row) {
    static const std::vector<int> rows[4] = {
        {29, 27, 6, 25, 5, 17, 16, 54, 55, 56},              // Z X C V B N M , . /
        {4, 22, 7, 9, 10, 11, 13, 14, 15, 51, 52},           // A S D F G H J K L ; '
        {20, 26, 8, 21, 23, 28, 24, 12, 18, 19, 47, 48},     // Q W E R T Y U I O P [ ]
        {30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 45, 46},    // 1 2 3 4 5 6 7 8 9 0 - =
    };
    static const std::vector<int> none;
    return row >= 0 && row < 4 ? rows[row] : none;
}

bool isAssignableComputerKey(int key) { return named(key) != nullptr; }

std::string computerKeyCode(int key) {
    const Named* n = named(key);
    return n ? n->code : "";
}

int computerKeyFromCode(const std::string& code) {
    for (const Named& n : kNamed)
        if (code == n.code) return n.key;
    return 0;
}

std::string computerKeyName(int key) {
    const Named* n = named(key);
    return n ? n->name : "";
}

int pianoComputerKey(int midiNote) {
    // White keys and black keys of each row, by semitone above its C. The Z
    // row stops at B3, as a note key holds one computer key.
    static const std::map<int, int> keys = [] {
        std::map<int, int> m;
        const auto& z = computerKeyRow(0);
        const auto& a = computerKeyRow(1);
        const auto& q = computerKeyRow(2);
        const auto& d = computerKeyRow(3);
        const int whites[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19};
        for (int i = 0; i < 7; ++i) m[48 + whites[i]] = z[i];                    // Z X C V B N M
        const std::pair<int, int> lowBlacks[] = {{1, 1}, {3, 2}, {6, 4}, {8, 5}, {10, 6}};   // S D G H J
        for (const auto& [semi, i] : lowBlacks) m[48 + semi] = a[i];
        for (int i = 0; i < 12; ++i) m[60 + whites[i]] = q[i];                   // Q ... ]
        const std::pair<int, int> highBlacks[] = {{1, 1}, {3, 2}, {6, 4}, {8, 5}, {10, 6},
                                                  {13, 8}, {15, 9}, {18, 11}};   // 2 3 5 6 7 9 0 =
        for (const auto& [semi, i] : highBlacks) m[60 + semi] = d[i];
        return m;
    }();
    const auto it = keys.find(midiNote);
    return it != keys.end() ? it->second : 0;
}

const std::vector<int>& computerKeyZigzag() {
    static const std::vector<int> zigzag = [] {
        std::vector<int> out;
        for (int pair = 0; pair < 2; ++pair) {
            const auto& low = computerKeyRow(2 * pair);
            const auto& high = computerKeyRow(2 * pair + 1);
            // Each low key, then the one up-right of it.
            for (std::size_t i = 0; i < low.size(); ++i) {
                out.push_back(low[i]);
                if (i + 1 < high.size()) out.push_back(high[i + 1]);
            }
        }
        return out;
    }();
    return zigzag;
}

std::map<int, Key> computerKeyMap(const Layout& layout) {
    std::map<int, Key> out;
    for (const Key& k : layout.keysIn(layout.bounds()))
        if (k.playable && k.computerKey != 0) out.emplace(k.computerKey, k);
    return out;
}

} // namespace kedo
