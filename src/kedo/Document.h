#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "kedo/KeyStyle.h"
#include "kedo/Layout.h"
#include "kedo/Performer.h"
#include "kedo/UmpOutput.h"

// Documents (decisions.md, "Documents with several layouts"): a .kedo bundle
// of layouts, each a LayoutSetup in a file of its own.
namespace kedo {

// One layout as the player sets it up: its tuning, keys, playing and MIDI
// settings. KeyEDO's whole document (KeyboardConfiguration.swift), as JSON
// with KeyEDO's field names, so a KeyEDO file is a layout here, and a layout
// file opens in KeyEDO (decisions.md, "Documents").
struct LayoutSetup {
    std::string  name;            // shown in the layout menu; KeyEDO ignores it
    Tuning       tuning;
    LayoutConfig layout{.pianoLowest = 36};   // C2, as KeyEDO's piano
    TileColor    tileColor = TileColor::Note;
    PlayConfig   play;
    OutputConfig output;          // group, channel, bend ranges, Per-Note Management
    bool         locked = false;

    // KeyEDO's destination, read from and written to KeyEDO's own files
    // only: in a .kedo bundle the destination is the Document's.
    std::string destination;

    // Fields this version doesn't use (KeyEDO's arpeggiator, hover
    // expression), as JSON text by key, written back unchanged on save.
    std::map<std::string, std::string> extras;

    bool operator==(const LayoutSetup&) const = default;
};

// From JSON text. std::nullopt (and `error` set) if it isn't a JSON object.
// Missing fields keep their defaults and out-of-range ones are clamped, so
// any KeyEDO document opens.
std::optional<LayoutSetup> parseLayoutSetup(const std::string& json, std::string* error = nullptr);

// To JSON text, keys sorted and indented as KeyEDO writes them. Every key
// KeyEDO reads is written (its defaults for ones this version doesn't use),
// so KeyEDO can open the file.
std::string writeLayoutSetup(const LayoutSetup& document);

std::optional<LayoutSetup> loadLayoutSetup(const std::string& path, std::string* error = nullptr);
bool saveLayoutSetup(const std::string& path, const LayoutSetup& document, std::string* error = nullptr);

// A document: the layouts chosen for it, in order, and where they play.
struct Document {
    std::vector<LayoutSetup> layouts{[] {   // never empty
        LayoutSetup first;
        first.name = "Layout 1";
        return first;
    }()};
    int current = 0;                  // the one showing
    std::string destination;          // a destination's id or name ("" for none)

    LayoutSetup&       layout() { return layouts[current]; }
    const LayoutSetup& layout() const { return layouts[current]; }

    bool operator==(const Document&) const = default;
};

inline constexpr const char* kDocumentFormat = "keyboardedo-document";
inline constexpr int kDocumentVersion = 1;

// A .kedo bundle (a folder: document.json and layouts/*.json), or a KeyEDO
// .json file, which opens as a document of one layout named after the file,
// its destination the document's. A bundle of a newer version, or a folder
// that isn't one, is refused. Layout files in layouts/ that document.json
// doesn't list come after the listed ones, by name, so a layout copied in
// from another document appears.
std::optional<Document> loadDocument(const std::string& path, std::string* error = nullptr);

// Writes `path` as a .kedo bundle: a complete new one beside it
// (<path>.tmp), then swapped into place, so a crash leaves the old bundle or
// the new one, never a mix. Refuses to replace anything that isn't a bundle.
bool saveDocument(const std::string& path, const Document& document, std::string* error = nullptr);

// True if `path` names a .kedo bundle (or would: it ends in .kedo).
bool isBundlePath(const std::string& path);

} // namespace kedo
