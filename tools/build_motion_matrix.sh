#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
stage=${1:?stage label required}
[[ $stage =~ ^[a-z0-9_-]+$ ]] || exit 2
mkdir -p "_experimental/$stage"
for profile in baseline off scurve ftm bench all; do
  ./tools/build_motion.sh "$profile" >"_experimental/$stage/$profile.log" 2>&1
  echo "$stage: $profile compiled"
done
