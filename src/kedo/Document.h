#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "kedo/Arpeggiator.h"
#include "kedo/Controls.h"
#include "kedo/KeyStyle.h"
#include "kedo/Layout.h"
#include "kedo/Performer.h"
#include "kedo/Scales.h"
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
    LayoutConfig layout{.pianoLowest = 36, .customKeys = {}};   // C2, as KeyEDO's piano
    TileColor    tileColor = TileColor::Note;
    PlayConfig   play;
    OutputConfig output;          // group, channel, bend ranges, Per-Note Management
    ArpConfig    arp;             // KeyEDO's arp* fields
    bool         locked = false;

    // KeyEDO's destination, read from and written to KeyEDO's own files
    // only: in a .kedo bundle the destination is the Document's.
    std::string destination;

    // Fields this version doesn't use (KeyEDO's hover expression and arp
    // rate source), as JSON text by key, written back unchanged on save.
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
    // The performance bar's knobs: per document, so switching layouts
    // doesn't change what they send (decisions.md, "Performance bar").
    std::vector<KnobAssignment> knobs = defaultKnobs();
    // The performance bar's scale, shown on the diamond piano's keys: per
    // document too (decisions.md, "Scale highlight").
    ScaleChoice scale;
    // The showing layout's notes held when the app quit, in the order
    // played (the arpeggiator's As Played follows it), by Note::id with their
    // velocities: latched (Shift, or the toolbar's Latch), or else kept by
    // the arpeggiator's Hold. Saved with the document (and the app's
    // session), so opening it plays on where it stopped.
    struct HeldNote {
        int id = 0;
        std::uint16_t velocity = 0;
        bool latched = false;
        bool operator==(const HeldNote&) const = default;
    };
    std::vector<HeldNote> held;

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

// The groups a document's notes go out on, 0-based, as one contiguous run
// (a function block's: groups 1 and 5 make 1-5): each layout's group, and a
// custom layout's keys' own (decisions.md, "Identity and discovery").
// `showing`, if given, stands for document.layout(): the settings being
// edited, not yet stored.
struct GroupSpan {
    std::uint8_t first = 0, count = 1;
    bool operator==(const GroupSpan&) const = default;
};
GroupSpan groupSpan(const Document& document, const LayoutSetup* showing = nullptr);

} // namespace kedo
