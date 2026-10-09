#pragma once

#include <map>
#include <optional>
#include <string>

#include "kedo/KeyStyle.h"
#include "kedo/Layout.h"
#include "kedo/Performer.h"
#include "kedo/UmpOutput.h"

// One saved keyboard setup: KeyEDO's document (KeyboardConfiguration.swift),
// as JSON with KeyEDO's field names, so its files open here and ours in it
// (decisions.md, "Documents").
namespace kedo {

struct Document {
    Tuning       tuning;
    LayoutConfig layout{.pianoLowest = 36};   // C2, as KeyEDO's piano
    TileColor    tileColor = TileColor::Note;
    PlayConfig   play;
    OutputConfig output;          // group, channel, bend ranges, Per-Note Management
    bool         locked = false;

    // Where it plays: a destination's id ("" for none), and whether it
    // publishes its own source.
    std::string destination;
    bool        publish = false;

    // Fields this version doesn't use (KeyEDO's arpeggiator, hover
    // expression), as JSON text by key, written back unchanged on save.
    std::map<std::string, std::string> extras;

    bool operator==(const Document&) const = default;
};

// From JSON text. std::nullopt (and `error` set) if it isn't a JSON object.
// Missing fields keep their defaults and out-of-range ones are clamped, so
// any KeyEDO document opens.
std::optional<Document> parseDocument(const std::string& json, std::string* error = nullptr);

// To JSON text, keys sorted and indented as KeyEDO writes them. Every key
// KeyEDO reads is written (its defaults for ones this version doesn't use),
// so KeyEDO can open the file.
std::string writeDocument(const Document& document);

std::optional<Document> loadDocument(const std::string& path, std::string* error = nullptr);
bool saveDocument(const std::string& path, const Document& document, std::string* error = nullptr);

} // namespace kedo
