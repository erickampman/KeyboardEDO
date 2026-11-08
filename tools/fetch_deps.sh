#!/usr/bin/env bash
#
# Initialise every dependency submodule KeyboardEDO needs, at the pinned commits.
#
# Run this once after a fresh `git clone`, and again after a `git pull` that
# moves, adds or re-points a submodule.
#
set -euo pipefail
cd "$(dirname "$0")/.."

echo "==> Submodule URLs (after a pull that changed one)"
git submodule sync --recursive

# The three third-party submodules come as one commit each: `shallow = true` in .gitmodules alone
# still fetches every branch and tag at depth 1 (a 502 MB checkout, not 133 MB). GitHub serves a
# pinned commit by its hash, so it needn't be a branch tip. uw-core stays a full clone, since it
# is worked on from here.
echo "==> Submodules: SDL, Dear ImGui, nlohmann/json (one commit each)"
git submodule update --init --depth 1 --single-branch external/SDL external/imgui external/json

echo "==> Submodules: uw-core (full history), and the rest"
git submodule update --init --recursive

echo "==> Done. Configure with: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release"
