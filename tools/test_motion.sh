#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
mkdir -p _experimental/logs
flags=(-std=gnu11 -O2 -g -ffunction-sections -fdata-sections -I main -I main/grbl)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
for profile in baseline scurve ftm all; do
  defs=()
  if [[ $profile == scurve || $profile == all ]]; then defs+=(-DENABLE_JERK_ACCELERATION=1 -DEXPERIMENTAL_S_CURVE=1); fi
  if [[ $profile == ftm || $profile == all ]]; then defs+=(-DEXPERIMENTAL_FTM=1); fi
  "${CC:-cc}" "${flags[@]}" "${defs[@]}" tests/motion/core_harness.c main/grbl/nuts_bolts.c main/experimental/scurve.c main/experimental/sample_motion.c \
    -Wl,--gc-sections -lm -o "_experimental/core-$profile"
  timeout 60 "_experimental/core-$profile" | tee "_experimental/logs/core-$profile.csv"
done

"${CC:-cc}" "${flags[@]}" -Wall -Wextra -Werror tests/motion/ftm_test.c main/experimental/ftm.c -lm -o _experimental/ftm-test
_experimental/ftm-test
for module in shaper smoothing; do
  "${CC:-cc}" "${flags[@]}" -Wall -Wextra -Werror "tests/motion/${module}_test.c" "main/experimental/$module.c" -lm -o "_experimental/$module-test"
  "_experimental/$module-test"
done
"${CC:-cc}" "${flags[@]}" -DEXPERIMENTAL_FTM=1 -DEXPERIMENTAL_INPUT_SHAPING=1 \
  -DEXPERIMENTAL_TRAJECTORY_SMOOTHING=1 -DEXPERIMENTAL_MOTION_DIAGNOSTICS=1 \
  tests/motion/diagnostics_test.c main/grbl/nuts_bolts.c main/experimental/ftm.c \
  main/experimental/shaper.c main/experimental/smoothing.c main/experimental/scurve.c main/experimental/sample_motion.c -Wl,--gc-sections -lm -o _experimental/diagnostics-test
_experimental/diagnostics-test
# Original acceleration acceptance gate remains unchanged and mandatory.
ENFORCE_ACCELERATION_GATE=1 _experimental/core-scurve

"${CC:-cc}" "${flags[@]}" -Wall -Wextra -Werror tests/motion/scurve_test.c main/experimental/scurve.c -lm -o _experimental/scurve-test
_experimental/scurve-test

"${CC:-cc}" "${flags[@]}" -DENABLE_JERK_ACCELERATION=1 -DEXPERIMENTAL_S_CURVE=1 \
  tests/motion/scurve_pipeline_test.c main/grbl/nuts_bolts.c main/experimental/scurve.c main/experimental/sample_motion.c \
  -Wl,--gc-sections -lm -o _experimental/scurve-pipeline-test
_experimental/scurve-pipeline-test

ENFORCE_ACCELERATION_GATE=1 _experimental/core-all
for curve in 0 1; do
  "${CC:-cc}" "${flags[@]}" -DEXPERIMENTAL_FTM=1 -DEXPERIMENTAL_S_CURVE=$curve -DENABLE_JERK_ACCELERATION=$curve \
    tests/motion/ftm_pipeline_test.c main/grbl/nuts_bolts.c main/experimental/scurve.c main/experimental/sample_motion.c \
    -Wl,--gc-sections -lm -o _experimental/ftm-pipeline-test
  _experimental/ftm-pipeline-test
done
for curve in 0 1; do
"${CC:-cc}" "${flags[@]}" -DEXPERIMENTAL_FTM=1 -DEXPERIMENTAL_INPUT_SHAPING=1 -DEXPERIMENTAL_TRAJECTORY_SMOOTHING=1 \
  -DEXPERIMENTAL_S_CURVE=$curve -DENABLE_JERK_ACCELERATION=$curve tests/motion/shaped_pipeline_test.c main/grbl/nuts_bolts.c \
  main/experimental/scurve.c main/experimental/sample_motion.c main/experimental/shaper.c main/experimental/smoothing.c \
  -Wl,--gc-sections -lm -o _experimental/shaped-pipeline-test
ENFORCE_ACCELERATION_GATE=1 _experimental/shaped-pipeline-test | tee _experimental/logs/core-shaped.csv
done

bash tools/check_motion_feature_combinations.sh
