#include "kedo/Preferences.h"

#include <fstream>
#include <random>

#include <nlohmann/json.hpp>

namespace kedo {

std::string makeProductInstanceId() {
    std::random_device random;
    std::string id;
    for (int i = 0; i < 16; ++i) id.push_back("0123456789abcdef"[random() % 16]);
    return id;
}

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
        flag("barOpen", p.barOpen);
        flag("wheelsOpen", p.wheelsOpen);
        flag("knobsOpen", p.knobsOpen);
        flag("arpOpen", p.arpOpen);
        flag("scaleOpen", p.scaleOpen);
        flag("editing", p.editing);
        flag("showComputerKeys", p.showComputerKeys);
        auto text = [&](const char* key, std::string& value) {
            if (j.contains(key) && j[key].is_string()) value = j[key].get<std::string>();
        };
        text("productInstanceId", p.productInstanceId);
        text("documentPath", p.documentPath);
    } catch (const nlohmann::json::exception&) {
    }
    return p;
}

bool savePreferences(const std::string& path, const Preferences& p) {
    const nlohmann::json j = {
        {"publish", p.publish}, {"productInstanceId", p.productInstanceId}, {"panelOpen", p.panelOpen}, {"monitorOpen", p.monitorOpen},
        {"barOpen", p.barOpen}, {"wheelsOpen", p.wheelsOpen}, {"knobsOpen", p.knobsOpen}, {"arpOpen", p.arpOpen},
        {"scaleOpen", p.scaleOpen}, {"editing", p.editing}, {"showComputerKeys", p.showComputerKeys},
        {"documentPath", p.documentPath}};
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) out << j.dump(2) << "\n";
    return bool(out);
}

} // namespace kedo
