#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
mkdir -p _experimental/logs
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Werror -I main tests/motion/benchmark.c \
 main/experimental/ftm.c main/experimental/shaper.c main/experimental/smoothing.c -lm -o _experimental/motion-benchmark
_experimental/motion-benchmark | tee _experimental/logs/benchmark.csv
