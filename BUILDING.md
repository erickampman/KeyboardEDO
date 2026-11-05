# Building KeyboardEDO

KeyboardEDO builds the same way on Linux and macOS: CMake + Ninja, git submodules, C++20. The build
makes the app (`KeyboardEDO.app` on macOS, `keyboardedo` on Linux), its core library and tests, and
two tools: `kedo-midi-listen` and `kedo-layout-svg`.

## What you need

**macOS:** macOS 26 or later on Apple Silicon (arm64). Intel Macs aren't supported.

- The Xcode Command Line Tools: `xcode-select --install`. They bring clang, the macOS SDK, git and
  Python 3.
- [Homebrew](https://brew.sh), then `brew install cmake ninja`.
- Full Xcode is optional: Xcode 26 or later compiles the app icon. Without it the app builds
  with no icon, and CMake says so.

**Linux:** a C++20 compiler (GCC 11+ or Clang 14+), CMake 3.21+, Ninja, Python 3 and git. On Debian
or Ubuntu this installs them and every library the build needs:

```bash
sudo apt install build-essential cmake ninja-build pkg-config python3 git \
  libgl1-mesa-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxinerama-dev \
  libxcb1-dev libx11-xcb-dev libxcb-util-dev libxkbcommon-dev libxkbcommon-x11-dev \
  libfontconfig1-dev libasound2-dev libjack-jackd2-dev \
  libxfixes-dev libxi-dev libxss-dev libxtst-dev libwayland-dev wayland-protocols \
  libegl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev libdecor-0-dev libudev-dev \
  libdbus-1-dev libibus-1.0-dev
```

It's one list for every Unlikelyware project (the same one CI installs), so it covers more than
KeyboardEDO needs. On other distros, install the same libraries under their own names.

For MIDI, Linux needs alsa-lib 1.2.10 or later (Ubuntu 24.04, Debian 13 or newer) and a 6.5 or later
kernel: UMP in the ALSA sequencer. Under GNOME on Wayland the window's title bar and borders come
from libdecor's GTK plugin (`libdecor-0-plugin-1-gtk`, a runtime package). SDL's own list of what it
can use is `external/SDL/docs/README-linux.md`.

To build only the core and its tests, configure with `-DKEDO_APP=OFF`; that needs none of the
libraries.

## Get the source

```bash
git clone https://github.com/erickampman/KeyboardEDO.git
cd KeyboardEDO
tools/fetch_deps.sh
```

`tools/fetch_deps.sh` checks out the git submodules at their pinned commits (uw-core, SDL, Dear
ImGui and nlohmann/json). Run it again after a `git pull` that moves, adds or re-points a submodule:
a plain pull leaves them where they were.

## Build and test

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The same three commands on both platforms. Everything goes in `build/` (ignored by git).

## Running it

On the Mac the build makes `build/KeyboardEDO.app`. `open build/KeyboardEDO.app` starts it as an
app, and `build/keyboardedo` is a link to the executable inside, for command lines. To keep it in
Applications (and after deleting the clone), copy the app there; an app you built yourself has no
quarantine flag, so it opens without the steps a downloaded one needs:

```bash
rm -rf /Applications/KeyboardEDO.app
cp -R build/KeyboardEDO.app /Applications/
```

On Linux, `cmake --install build` (to `/usr/local`; `--prefix ~/.local` for yourself) installs
`keyboardedo`, its help page, a desktop entry and the icons, so it appears in the desktop's app list
with its icon.

```bash
cmake --install build --prefix ~/.local
```

```bash
./build/keyboardedo
./build/keyboardedo --edo 17 --layout tiled --publish
./build/keyboardedo ~/Documents/"My set.kedo"
./build/keyboardedo ~/"Library/Mobile Documents/com~apple~CloudDocs/KeyEDO/Untitled.json"   # imported
```

`keyboardedo --help` lists the options. To see what it sends without a DAW, run this alongside it
with Publish on:

```bash
./build/kedo-midi-listen KeyboardEDO 60
```

To see the layouts without a UI:

```bash
./build/kedo-layout-svg build/svg
```

VS Code's and VSCodium's snaps leak their GTK environment into their terminals (`GDK_BACKEND=x11`,
their own GSettings schemas), which leaves a window that can't be moved or resized, or aborts in
GLib. The app restores the originals the snap saves as `NAME_VSCODE_SNAP_ORIG`, and unsets a
`GDK_BACKEND` that excludes Wayland.

## Options

Pass these to the first command (`cmake -B build ... -DNAME=VALUE`).

| Option | Default | Effect |
|---|---|---|
| `KEDO_APP` | `ON` | `OFF` builds only the core library and the tests, without SDL. |
| `KEDO_TESTS` | `ON` when built on its own | Build the tests. |
| `KEDO_SYSTEM_SDL` | `OFF` | Use an installed SDL3 instead of `external/SDL`. |
| `KEDO_UW_CORE_DIR` | `external/uw-core` | Build against another uw-core clone (below). |

## Building against a local uw-core

To try a uw-core change that isn't pushed and pinned yet, point the build at another clone:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DKEDO_UW_CORE_DIR=../uw-core
```

When the change is ready, push uw-core first, then move KeyboardEDO's submodule pin
(uw-core's README, "Pushing: uw-core first").

## How this differs from the other projects

Every Unlikelyware project builds the same way: `git clone`, `tools/fetch_deps.sh`, the three
commands under "Build and test", and for a plugin `cmake --build build --target install-clap`. The
same package list, the same `build/` folder, the same option names (`<PROJECT>_BUILD_VST3`,
`<PROJECT>_UW_CORE_DIR` and so on). What's different here, and why:

- The UI is SDL3 and Dear ImGui, not DPF, so it has SDL as a submodule (large: most of a clone's size) and `KEDO_SYSTEM_SDL` to use an installed SDL3 instead.
- An app, not a plugin: no `install-clap`. On Linux it installs with `cmake --install` (as StageHand does); on macOS it's `build/KeyboardEDO.app`, copied to Applications to keep.
- The option prefix is `KEDO_`, the short name its code uses throughout.
- The app icon on macOS needs full Xcode 26 (as StageHand's); without it the app builds with no icon.
- No `_WERROR` option: the seven synths have one and their CI turns it on; this project's build still has a few warnings in its own sources, so it comes once they're fixed.
- CI (`.github/workflows/ci.yml`) runs on `ubuntu-24.04`, the oldest Ubuntu supported, not `ubuntu-latest`, and on `macos-26`.
