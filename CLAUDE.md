# KeyboardEDO

A cross-platform, UMP-first rewrite of KeyEDO (`~/Dev-MIDI/KeyEDO`). Start
with README.md, then [docs/decisions.md](docs/decisions.md) and
[docs/roadmap.md](docs/roadmap.md): the roadmap says which milestone is next.

Related repos:
- **KeyEDO** (`~/Dev-MIDI/KeyEDO`): the behaviour guide. Its
  `docs/controller-roadmap.md` lists the MIDI 2.0 features to implement.
- **uw-core** (`~/Dev-Common/uw-core`, submodule at `external/uw-core`): UMP
  types and the MIDI backends. It has no UI code.
- **Umpire** (`~/Dev-MIDI/UmpireDAW`): its `docs/decisions.md` has the
  shared-code and DPF-fork decisions this repo follows.

## Working with the author

- One milestone per session, about a day of work each.
- Keep `kedo::core` free of toolkit types; only `src/app/` knows SDL3 and
  ImGui.
- Check UI changes with `keyboardedo --snapshot FILE.png [--click X,Y]`
  rather than asking the author to look.
- Keep tests silent (no audible output).
- Record decisions in the docs, not only in conversation.
- The author commits to `main` directly. Never open pull requests. Ask before
  pushing. Push uw-core before a pin that points at it
  (`git push --recurse-submodules=check`).
