#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
mkdir -p _experimental
for defs in '' '-DEXPERIMENTAL_FTM=1' '-DEXPERIMENTAL_FTM=1 -DEXPERIMENTAL_INPUT_SHAPING=1' '-DEXPERIMENTAL_FTM=1 -DEXPERIMENTAL_TRAJECTORY_SMOOTHING=1' '-DEXPERIMENTAL_FTM=1 -DEXPERIMENTAL_INPUT_SHAPING=1 -DEXPERIMENTAL_TRAJECTORY_SMOOTHING=1'; do
  cc -std=gnu11 -O2 -Wall -Wextra -Werror -I main -I main/grbl $defs -DEXPERIMENTAL_MOTION_DIAGNOSTICS=1 -c main/experimental/diagnostics.c -o _experimental/diagnostics-combination.o
  cc -std=gnu11 -O2 -Wall -Wextra -Werror -I main $defs -c main/experimental/sample_motion.c -o _experimental/sample-combination.o
 done
echo 'Independent diagnostics/filter compilation combinations passed without warnings'
