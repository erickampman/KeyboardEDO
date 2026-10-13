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
        auto flag = [&](const char* key, bool& value) {
            if (j.contains(key) && j[key].is_boolean()) value = j[key].get<bool>();
        };
        flag("publish", p.publish);
        flag("panelOpen", p.panelOpen);
        flag("monitorOpen", p.monitorOpen);
        flag("editing", p.editing);
        flag("showComputerKeys", p.showComputerKeys);
    } catch (const nlohmann::json::exception&) {
    }
    return p;
}

bool savePreferences(const std::string& path, const Preferences& p) {
    const nlohmann::json j = {
        {"publish", p.publish}, {"panelOpen", p.panelOpen}, {"monitorOpen", p.monitorOpen},
        {"editing", p.editing}, {"showComputerKeys", p.showComputerKeys}};
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) out << j.dump(2) << "\n";
    return bool(out);
}

} // namespace kedo
