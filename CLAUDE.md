# KeyboardEDO

A cross-platform, UMP-first rewrite of KeyEDO (`~/Dev-MIDI/KeyEDO`). Start
with README.md, then [docs/decisions.md](docs/decisions.md) and
[docs/roadmap.md](docs/roadmap.md): the roadmap says which milestone is next.

Related repos:
- **KeyEDO** (`~/Dev-MIDI/KeyEDO`): the behaviour guide. Its
  `docs/controller-roadmap.md` lists the MIDI 2.0 features to implement.
- **uw-core** (`~/Dev-Common/uw-core`, submodule at `external/uw-core`): UMP
  types and the MIDI backends. It has no UI code. **Change it only in
  `~/Dev-Common/uw-core`**: commit, test and push there, then
  `git -C external/uw-core pull` and commit the pin here. Never commit inside
  `external/uw-core` (decisions.md, "Working method").
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

## On the Linux PC

KeyboardEDO may never have been built there; check with the author. Everything so far was done and
checked on the Mac (roadmap milestones 1 to 6, except 3b). To start:

1. Clone with submodules (README, "Building and testing"), and set up the json submodule for
   `push.recurseSubmodules check` as README says, once per clone.
2. Paths there are lower-case under `~/dev/`: uw-core is `~/dev/dev-common/uw-core` (clone it if it's
   missing; changes are made there, never in `external/uw-core`), Umpire `~/dev/dev-midi/UmpireDAW`,
   StageHand `~/dev/dev-audio/StageHand`, and the synths `~/dev/dev-clap/<name>`. KeyboardEDO
   itself probably belongs in `~/dev/dev-midi/KeyboardEDO`; ask the author.
3. Build the core and tests first with `-DKEDO_APP=OFF`; they need no SDL dependencies. Then the app,
   after installing SDL3's X11 and Wayland build dependencies (`external/SDL/docs/README-linux.md`).
4. `ctest --test-dir build --output-on-failure`, then `keyboardedo --snapshot FILE.png` to check
   the three keyboards draw.

Then roadmap milestone 2's "Still to check by hand" Linux items (X11, Wayland, two-finger touch), and
milestone 3b, the ALSA sequencer output in uw-core, which is Linux-only work: until it lands the app
plays to its monitor only. Record what you find in the roadmap and decisions.md. The synths to test
against are in decisions.md, "What the synths do with it"; on Linux, play them in StageHand
(Linux-first).
