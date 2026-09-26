#!/usr/bin/env bash
# Fresh, isolated ESP-IDF builds. No upload, serial, or original-build access.
set -euo pipefail
project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
profile=${1:-off}
case "$profile" in
  baseline) flags=();;
  off) flags=(-DEXPERIMENTAL_MOTION_DIAGNOSTICS=OFF -DEXPERIMENTAL_S_CURVE=OFF -DEXPERIMENTAL_FTM=OFF -DEXPERIMENTAL_INPUT_SHAPING=OFF -DEXPERIMENTAL_TRAJECTORY_SMOOTHING=OFF);;
  scurve) flags=(-DEXPERIMENTAL_MOTION_DIAGNOSTICS=OFF -DEXPERIMENTAL_S_CURVE=ON -DEXPERIMENTAL_FTM=OFF -DEXPERIMENTAL_INPUT_SHAPING=OFF -DEXPERIMENTAL_TRAJECTORY_SMOOTHING=OFF);;
  ftm) flags=(-DEXPERIMENTAL_MOTION_DIAGNOSTICS=OFF -DEXPERIMENTAL_S_CURVE=OFF -DEXPERIMENTAL_FTM=ON -DEXPERIMENTAL_INPUT_SHAPING=OFF -DEXPERIMENTAL_TRAJECTORY_SMOOTHING=OFF);;
  bench) flags=(-DEXPERIMENTAL_MOTION_DIAGNOSTICS=ON -DEXPERIMENTAL_S_CURVE=OFF -DEXPERIMENTAL_FTM=ON -DEXPERIMENTAL_INPUT_SHAPING=ON -DEXPERIMENTAL_TRAJECTORY_SMOOTHING=ON);;
  all) flags=(-DEXPERIMENTAL_MOTION_DIAGNOSTICS=ON -DEXPERIMENTAL_S_CURVE=ON -DEXPERIMENTAL_FTM=ON -DEXPERIMENTAL_INPUT_SHAPING=ON -DEXPERIMENTAL_TRAJECTORY_SMOOTHING=ON);;
  *) echo 'Usage: tools/build_motion.sh baseline|off|scurve|ftm|bench|all' >&2;exit 2;;
esac
# Existing IDF 4.4 Kconfig cannot handle spaces. CMake retains this logical path.
alias_root="/tmp/grblhal-experimental-$UID"
if [[ ! -e "$alias_root" && ! -L "$alias_root" ]]; then ln -s -- "$project_root" "$alias_root"; fi
[[ $(readlink -f -- "$alias_root") == "$project_root" ]] || { echo 'Build alias points to another project' >&2;exit 1; }
[[ -f "$project_root/docs/EXPERIMENTAL_MOTION.md" ]] || exit 1
sdk_root=${EXPERIMENTAL_IDF_PATH:-/home/robert/esp/esp-idf}
set +u
source "$sdk_root/export.sh"
set -u
source_root="$alias_root"
if [[ $profile == baseline ]]; then
  source_root="$alias_root/_experimental/original-source"
  if [[ ! -f "$source_root/CMakeLists.txt" ]]; then
    mkdir -p "$source_root"
    git -C "$project_root" archive original-baseline main CMakeLists.txt sdkconfig sdkconfig.defaults sdkconfig.defaults.esp32s3 webui partitions.csv partitions_s3_8m.csv partitions_s3_16m.csv dependencies.lock | tar -x -C "$source_root"
  fi
fi
cmake -S "$source_root" -B "$alias_root/_experimental/build-$profile" -G Ninja \
  -DIDF_TARGET=esp32s3 -DPYTHON_DEPS_CHECKED=1 "${flags[@]}"
cmake --build "$alias_root/_experimental/build-$profile" -j "${BUILD_JOBS:-4}"
