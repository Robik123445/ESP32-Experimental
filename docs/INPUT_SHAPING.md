# ZV input shaping

## Live pipeline and persistent `$` settings

`EXPERIMENTAL_INPUT_SHAPING` requires FTM. It is connected in `sample_motion.c`,
after foreground 1 kHz interpolation and before native segment publication and
STEP/DIR. Its default is OFF on both axes. No filter math runs in STEP ISR.

Build the `bench` or `all` profile to compile FTM, diagnostics, ZV settings and
optional smoothing. `$780`–`$786` are persistent settings listed by `$$`, saved to
the controller's NVS, and applied only while Idle:

| Setting | Meaning | Default |
|---|---|---:|
| `$780` | X shaper type: 0 Off, 1 ZV | 0 |
| `$781` | X resonance frequency (Hz) | 0 |
| `$782` | X damping ratio | 0 |
| `$783` | Y shaper type: 0 Off, 1 ZV | 0 |
| `$784` | Y resonance frequency (Hz) | 0 |
| `$785` | Y damping ratio | 0 |
| `$786` | Smoothing window; 1 disables smoothing | 1 |

Enter measured/tentative parameters before turning a shaper on; invalid settings
are rejected without replacing the previously applied configuration. For example,
with the `all` profile compiled:

```
$781=40
$782=0.1
$784=63
$785=0.12
$786=8
$780=1
$783=1
$$
```

Use `$780=0` or `$783=0` to turn off each axis shaper; use `$786=1` to bypass
smoothing. The old `$EMCONFIG=Xtype,XHz,Xdamping,Ytype,YHz,Ydamping,window`
command remains as an idle-only convenience and saves to the same NVS settings.
These parameters persist through reboot. `$` settings cannot enable a feature
omitted at compile time; `$780`/`$783` only accept ZV if input shaping was built,
and windows above 1 only work when trajectory smoothing was built.


## Mathematics and geometry

For resonance f and damping z, let `r=sqrt(1-z*z)`,
`K=exp(-pi*z/r)`. ZV impulses are `(time=0, weight=1/(1+K))` and
`(time=1/(2*f*r), weight=K/(1+K))`. Fractional delays use linear interpolation.
Weights are nonnegative and sum to one: velocity and acceleration envelopes of a
bounded input remain bounded, and the final constant position is unchanged.
Fractional sampling approximates cancellation; no measured machine attenuation is
claimed. Host unit tests check sampled frequency response and DC gain.

Independent X/Y filtering bends a diagonal. The **live adapter instead composes
ZV kernels for moving X/Y axes and applies that common kernel to all three axes**.
A single-axis move uses its axis kernel. An XY line uses their convolution (up to
four impulses), addressing both resonances while preserving the straight line
before quantization. Z has no resonance parameter but shares the common time law.
The portable `shaper.c` also supports independent axis filtering for comparison;
that mode is not the live Cartesian adapter's geometry policy.

Every planner block ends at rest and drains its tail before the next block begins.
Thus corners are exact, not rounded. This costs throughput on dense short-line
and arc tessellations. Quantization uses absolute steps; each sampled coordinate
is within half a step of the continuous line, with additional within-sample
Bresenham rasterization. Exact endpoint and absolute pulse counts are tested.

The combined maximum delay must fit 511 samples; damping must be finite in [0,1),
frequency positive and below 500 Hz. Invalid compound delays are rejected without
changing the previous valid configuration. Optional common moving-average
smoothing adds window-1 tail samples (window 1..64) and likewise preserves lines.

## Stop behavior and evidence

Feed hold uses the existing bounded braking source and then drains the finite
filter tail. Shaping therefore adds stop latency (up to the configured tail;
maximum accepted delay plus smoothing is 574 ms). Fast cancel of sampled normal
motion aborts instead; reset clears queued sample/filter state. Homing, probing,
jog, synchronized spindle/laser and metadata-specialized motion use the native
path. No delayed pulse may be replayed automatically after an underrun.

`shaped_pipeline_test.c` runs actual planner/preparer/ISR endpoint and absolute
pulse tests with ZV alone and ZV+8-sample smoothing, including hold/resume, reversal,
low/high feeds, 10 m travel, tiny blocks, acceleration extremes, late streaming
and override reduction. It independently checks continuous XYZ line geometry,
velocity/acceleration bounds, quantization and finite tail completion. Tests use
a fake HAL and never access a physical machine.
