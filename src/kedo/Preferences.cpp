#include "kedo/Preferences.h"

#include <fstream>

#include <nlohmann/json.hpp>

namespace kedo {

Preferences loadPreferences(const std::string& path) {
    Preferences p;
    std::ifstream in(path, std::ios::binary);
    if (!in) return p;
    try {
        const auto j = nlohmann::json::parse(in);
        if (j.contains("publish") && j["publish"].is_boolean()) p.publish = j["publish"].get<bool>();
    } catch (const nlohmann::json::exception&) {
    }
    return p;
}

bool savePreferences(const std::string& path, const Preferences& p) {
    const nlohmann::json j = {{"publish", p.publish}};
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) out << j.dump(2) << "\n";
    return bool(out);
}

} // namespace kedo
