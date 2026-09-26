# ESP32 Experimental

Independent repository and full snapshot of `/home/robert/ESP32`. The root commit
and tag `original-baseline` preserve the original. No original files were modified,
no firmware was flashed and no physical motion was performed.

**Software integration and host tests complete; physical acceptance pending.**
The jerk timing defect is repaired. Safe analytic S-curve, 1 kHz FTM, configurable
ZV and optional smoothing now reach the native STEP/DIR pipeline for normal XYZ
motion. All features default OFF; filters default bypass even in enabled builds.
FTM conservatively stops and drains at every planner block. Specialized CNC modes
retain the native path. See the documented limitations before any hardware work.

- [Verified architecture](docs/MOTION_ARCHITECTURE.md)
- [Repair, integration and safety contracts](docs/EXPERIMENTAL_MOTION.md)
- [ZV configuration and geometry](docs/INPUT_SHAPING.md)
- [Measured results and remaining bench work](docs/BENCHMARKS.md)

```
./tools/build_motion.sh off
./tools/build_motion.sh all
./tools/build_motion_matrix.sh motion-final
./tools/test_motion.sh
SANITIZE=1 ./tools/test_motion.sh
./tools/benchmark_motion.sh
```

Profiles: baseline, off, scurve, ftm, bench, all. `bench` now includes live FTM
with S-curve OFF; the `$EMBENCH` command itself remains a no-GPIO benchmark.
Never invoke copied build caches: they contain original absolute paths. The
helpers use isolated `_experimental` builds. No remote is configured; former
submodules are vendored, and the original firmware remains the fallback.
