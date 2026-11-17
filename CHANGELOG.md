# Changelog

All notable changes to KeyboardEDO. Versions follow the git tags; the format is
[Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

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
