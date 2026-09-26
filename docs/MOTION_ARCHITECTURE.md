# Motion architecture — local firmware audit

## Final adapter audit — 2026-09-26

Live data ownership and callsites are described below. `driver.c` exports the
actual minimum timer period for FTM's foreground admission check; no STEP ISR
floating-point calculation was introduced. The native FIFO publishes sample
blocks with release/acquire fences. Idle-only diagnostics snapshot live counters
and foreground preparation time, with a last-64 planned/shaped velocity trace. The
ZV and smoothing parameters are registered as grblHAL `$780`–`$786` settings and
stored through the driver's NVS buffer.
Timer/period/range guards abort through the existing alarm mechanism. The target
backend is timer/RMT; I2S timing quantization is explicitly unsupported by FTM.
Host tests cover exact timer sums, starvation, hold and parking save/restore,
and native-path selection for jog/homing/probe. Physical timing remains unmeasured.


## Live filter ownership (2026-09-26)

`sm_begin` initializes the common X/Y ZV kernel at each rest boundary; `sm_sample`
filters continuous positions, drains delay tails and rounds absolute coordinates.
The block is discarded only after tail completion. Optional smoothing shares the
same time kernel across XYZ. No corner blending or independent-axis geometric
distortion is introduced. `$EMCONFIG` now configures this live foreground adapter,
while `$EMBENCH` remains a separate no-GPIO reference benchmark.


## Integrated fixed-grid adapter (2026-09-26)

Normal Cartesian path with FTM enabled:
`planner (rest boundaries) -> analytic profile -> 1 ms sample_motion positions ->
segment_buffer + immutable sample_blocks -> Bresenham ISR -> original ESP32 HAL`.
`ftm.c` is still a separate portable event-queue reference/benchmark, not a second
hardware consumer. The actual integration deliberately reuses native segment
ownership and native STEP/DIR pulse handling. S-curve is independent: FTM without
S-curve evaluates an analytic trapezoid. Configurations with all flags OFF retain
the original compiled motion paths. See the live FTM section in EXPERIMENTAL_MOTION.


Source of truth: complete local snapshot of `/home/robert/ESP32`, baseline
`dcc2b3de27e3d813959861dd05be12f7739651f5` (2026-09-25). This is not an audit
of current upstream. Core declares GRBL_BUILD 20250910, HAL_VERSION 10.
All paths below are relative to this independent repository.

## Original baseline command and execution path

1. `main/uart_serial.c` and `main/usb_serial.c` supply stream callbacks;
   `main/grbl/protocol.c:protocol_main_loop` reads `hal.stream.read()` and
   dispatches complete lines to `gc_execute_block` (`main/grbl/gcode.c`).
   This board config sets USB_SERIAL_CDC=0, so the native USB CDC backend is
   disabled; the UART/USB-serial transport remains as configured. Realtime bytes are intercepted by stream handlers and the protocol realtime
   machinery; they are not ordinary queued G-code.
2. `gc_execute_block` retains CNC modal state, offsets, units and spindle state.
   Linear motion calls `mc_line`; arcs are broken into lines in `mc_arc`
   (`main/grbl/motion_control.c`). `mc_line` performs limit/buffer/realtime
   handling, then calls `plan_buffer_line`. Keep this entry path unchanged.
3. `main/grbl/planner.c:plan_buffer_line` rounds absolute target millimetres
   to integer steps using `lroundf(target * steps_per_mm)`. It records per-axis
   unsigned steps, direction bits, dominant-axis step_event_count, length,
   feed, spindle metadata, entry speed and junction limits in `plan_block_t`.
   `plan_reset` allocates a doubly linked circular array with
   `settings.planner_buffer_blocks + 1` entries (default 100 usable blocks).
   `block_buffer_head`, `block_buffer_tail`, `block_buffer_planned` and
   `next_buffer_head` delimit producer/consumer/lookahead state.
4. `planner_recalculate` does reverse deceleration and forward acceleration
   passes over squared speeds. Junction deviation limits a *speed*, not a
   geometric corner blend. Collinear moves retain speed, reversal approaches
   zero junction speed. Axis limits are projected onto the line unit vector.
5. `main/grbl/stepper.c:st_prep_buffer` runs in foreground, called by the
   realtime protocol and motion-control paths. It checks out a planner block,
   copies immutable Bresenham/spindle/output metadata into `st_block_buffer`,
   calculates acceleration/cruise/deceleration and fills `segment_buffer`.
   The planner block may be released before those steps have executed.
6. `segment_buffer` has 10 entries (9 usable); `st_block_buffer` has 9.
   DT_SEGMENT = 1/(100*60) minutes by default: nominally 10 ms. It is NOT a
   strict fixed-time grid: block ends truncate it, very low feeds extend it
   until there is a whole step, and partial-step time carries to the next
   segment. About 90 ms of nominal segments is possible, not guaranteed.
   Each `segment_t` stores n_step, cycles_per_tick, AMASS level, current_rate,
   ramp type and execution-block reference, plus spindle synchronization data.
7. `main/driver.c:stepper_driver_isr` clears/rearms Timer Group 0's timer alarm
   and invokes `hal.stepper.interrupt_callback`, registered to
   `stepper_driver_interrupt_handler` in the core. This integer Bresenham ISR
   consumes segments, programs `hal.stepper.cycles_per_tick`, checks probing,
   applies homing axis masks and updates `sys.position` per step. AMASS changes
   ISR subdivision at low step rates, preserving total steps.
8. `hal.stepper.pulse_start` maps to `stepperPulseStart`. With the selected
   `BOARD_GENERIC_S3` and non-I2S configuration, direction uses GPIO and STEP
   pulses use RMT via `set_step_outputs`; RMT encodes pulse delay/width. The
   timer determines pulse onset cadence. `stepperCyclesPerTick` clamps the
   period to pulse limits and timer range. I2S is an alternate driver path,
   not the active board backend. Keep these timing responsibilities intact.

## Acceleration already present

`main/grbl/config.h` defaults ENABLE_JERK_ACCELERATION to Off. When enabled,
planner.c projects axis jerk and maximum acceleration onto each line, estimates
an effective acceleration for its existing lookahead, and stepper.c changes
`last_segment_accel` by jerk*time per prepared segment. Axis jerk settings are
$800/$801/$802 in mm/s^3; internal units are mm/min^3. Normal acceleration uses
mm/min^2 and velocity mm/min. This is an approximate segment-based third-order
profile, not a proven exact seven-phase S-curve solver. Its ramp transitions
clamp velocity and distance. Hold, override, reset and short blocks require
explicit regression tests; merely defining the macro is insufficient.

The baseline static `last_segment_accel` is not cleared by `st_reset`, while
other preparation state is. A reset in an acceleration ramp can therefore
carry acceleration into the next motion. Any local fix must be flag-gated.

## Safety and concurrency boundaries

- Foreground changes unprepared planner state; ISR owns execution and actual
  step position. Published segments must not be casually rewritten.
- `st_update_plan_block_parameters` callers
  invalidate preparation after lookahead/hold changes. Fast hold can rewind
  prepared segments using stored per-segment rate. Parking saves/restores
  preparation state. Extra filter state must participate in these operations.
- Empty segment queue invokes `st_go_idle` and EXEC_CYCLE_COMPLETE. A new FTM
  queue must distinguish normal end-of-stream from starvation; replaying the
  last event indefinitely is forbidden.
- Probe capture happens at step execution, not preparation. Homing masks are
  applied in the ISR. Abort resets execution through existing system logic.
- Laser dynamic PWM, CSS, spindle-synchronized motion, queued output commands
  and messages belong to execution blocks. Delaying steps alone would desync
  these features. A live per-axis event backend must carry their timing too.
- No stream writes, allocation, filtering or trigonometry belong in STEP ISR.
  CPU affinity, interrupt latency and RMT setup/hold need physical measurement.

## Build identity and isolation

The existing build metadata selects ESP-IDF v4.4.6, esp32s3 and
`/home/robert/esp/esp-idf`. PlatformIO's checked-in default targets an older
ESP32 board and is not the correct S3 baseline build. Rebuild with the existing
sdkconfig into a fresh directory; copied build caches contain original absolute
paths and MUST NOT be used to rebuild. ESP-IDF 4.4 Kconfig splits paths at spaces,
so a verified no-space symlink to this copy is used by the build helper.

The full tree, including ignored local files/build artifacts, was copied and
SHA-256 compared before Git reinitialization. Old Git metadata was archived
outside the source copy and replaced; formerly nested submodules are vendored
ordinary files so changes are independently reviewable. No original fetch,
checkout, clean, build, flash or serial connection is required.

## 2026-09-26 S-curve repair update

The experimental planner now solves jerk-aware reachable speed with the same
analytic phase integrals used by foreground step preparation (`scurve.[ch]`).
Legacy source is retained for baseline fidelity; directly enabling its unsafe
jerk branch without EXPERIMENTAL_S_CURVE is rejected at compilation. The unchanged acceleration
regression now takes 1.001756 s and passes; analytic tests cover 1,051 profiles.
Full build-matrix and subsequent FTM results are recorded in BENCHMARKS.md and
evidence/motion-final, separately from the historical failure. No target CPU load is inferred
from host timings and no device has been flashed.

The experimental planner's reverse/forward traversal covers all queued blocks:
jerk transition distances are not additive in squared velocity. A prepared
S-curve pins its exit through `st_curve_locked_exit` while late G-code adds
future blocks. Explicit control replans invalidate the lock. Float conversion
is checked against the reachable speed actually reconstructed by the stepper.
