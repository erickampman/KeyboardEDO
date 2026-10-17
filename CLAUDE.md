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
- Check UI changes with `keyboardedo --snapshot FILE.png [STEP...]`
  rather than asking the author to look. Steps run in order before the
  picture: `--click`, `--tap [shift+]X,Y`, `--double`, `--right` (a right-click), `--drag X1,Y1,X2,Y2`,
  `--key [ctrl+][shift+]NAME`, `--press NAME` and `--release NAME` (a key
  held and let go, e.g. `z`, `"left ctrl"`), `--move X,Y` (`--help`). While
  steps run, the real mouse is ignored. A snapshot run starts from the default preferences (Edit mode,
  not publishing: add `--publish`) and saves none, so it never changes the
  author's. The author may have KeyboardEDO running too (publishing the same
  name): to listen to a test instance, find its ALSA client by pid in
  `aconnect -l` rather than by name.
- Keep tests silent (no audible output).
- Record decisions in the docs, not only in conversation.
- The author commits to `main` directly. Never open pull requests. Ask before
  pushing. Push uw-core before a pin that points at it
  (`git push --recurse-submodules=check`).

## On the Linux PC

KeyboardEDO is at `~/dev/dev-midi/KeyboardEDO` and was first built there 2026-10-12 (roadmap
milestone 2): the core, tests and app, under Wayland and X11, with no code changes. README lists the
Ubuntu packages SDL needs.

- Paths there are lower-case under `~/dev/`: uw-core is `~/dev/dev-common/uw-core` (changes are made
  there, never in `external/uw-core`), Umpire `~/dev/dev-midi/UmpireDAW`, StageHand
  `~/dev/dev-midi/StageHand`, and the synths `~/dev/dev-clap/<name>`.
- `SDL_VIDEO_DRIVER=x11` or `wayland` chooses the video driver for `--snapshot` checks; SDL picks
  Wayland by default.
- Set up the json submodule for `push.recurseSubmodules check` as README says, once per clone.

It plays there through uw-core's `AlsaSeqOutput` (milestone 3b, 2026-10-12). Still to do by hand:
roadmap milestone 2's Linux checks (two-finger touch, a real mouse), and 3b's real synths. Record
what you find in the roadmap and decisions.md. The synths to test against are in decisions.md,
"What the synths do with it"; on Linux, play them in StageHand (Linux-first).
