# Changelog

All notable changes to KeyboardEDO. Versions follow the git tags; the format is
[Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Added

- Several fingers at once on a Mac touchscreen, each its own note. The bar's controls still
  need the trackpad or mouse there.
- Arpeggiator rests: right-click a note the arpeggiator is playing, or press Alt (Option on
  the Mac) with its computer key, and its steps keep their time but play nothing. An orange
  slash crosses its key. Rests are saved with the held notes.
- Glide slew: with Glide on, the Slew setting (Playing) slides a glide from key to key at a
  rate in cents a second instead of jumping. Off by default; saved in the document.

### Changed

- Help opens the help page in a window of the app's own, not in the web browser. Nothing is
  written to the temp folder, and on Linux no web server runs in the host any more.

## [0.9.0] - 2026-11-14

First public preview.

### Added

- On-screen MIDI 2.0 keyboard for any equal division of the octave from 5 to 72 steps.
- Exact pitch on every note as MIDI 2.0 Pitch 7.9, not pitch-bend tricks.
- A standard piano at 12-EDO, a diamond piano at other EDOs, and tiled layouts.
- Several layouts in one `.kedo` document, each with its own tuning; play with a mouse, trackpad,
  touchscreen or the computer keyboard.
- App for Linux (x86-64) and macOS 26 or later (Apple Silicon).

### Known gaps

- See the README's Known gaps section.
