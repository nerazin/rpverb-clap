# RPVerb Reconstruction (CLAP)

A CLAP reverb that reproduces the DSP of Rob Papen's RP-Verb RE 1.0.7 Rack Extension. The DSP was
ported by hand from the Rack Extension's x86-64 code, operation by operation, and checked against
the original binary running under Rosetta. Private project; not affiliated with Rob Papen or
Reason Studios.

## Controls

Every panel control is a parameter, with the original property tag as its CLAP id:
Power (Off / On / Bypass), Reset, Dry/Wet; Reverb (on, pre-delay, pre-delay disorder, space size,
space disorder, length, damping, volume, diffusion, low-pass, high-pass, Space Type, Colour);
Early and Late reflections; Envelope (mode, mono input, latch, attack, hold, release, amount);
Ensemble; Distortion; EQ; and the back panel's HQ Mode and Reset on Stop. "Side Input" stands
in for plugging a cable into the back-panel side inputs: when on, the plugin's second stereo
input drives the envelope.

Host modulation of Dry/Wet, Ensemble Volume, Distortion Amount, Early Volume, Late Volume,
Space Size, Reverb Length, Diffusion and Reverb Damping drives the matching CV input, as a CV
cable does in Reason. Other parameters are automatable but not modulatable.

## How it behaves (as the original does)

- The device works in 64-frame batches, so the plugin has 64 frames of latency. Parameter
  changes take effect at the next batch.
- Eleven or more parameter changes in one batch, a sample-rate change, the Reset control, and
  the plugin's first batch start a mute-and-clear of about 96 batches (about 140 ms at 44.1 kHz)
  during which the output, dry signal included, fades out.
- After 2000 batches of silent input and output the device stops processing until input returns.
- The envelope buttons keep the original's wiring: "Space Size" modulates what the Reverb Length
  knob controls, "Reverb Length" modulates what the Space Size knob controls, and
  "Size + Length + Audio" changes only the audio level.
- HQ Mode off switches off the second of the three reverbs and raises the rest by 1.5x.
- The output is multiplied by 1.15 and hard-clipped to +-1.

## Accuracy

| Check | Result |
| --- | --- |
| Every ported function against the original, same start state, all state words | 0 differ (42 functions, about 9,500 checks per 300-batch seed) |
| Whole device against the original's render, outputs and the full 26 MB state each batch | 0 differ (8 seeds x 5,000 batches) |
| Golden test (`tests/golden.cpp`), 2 x 6,000 batches, hashes measured on the original | x86-64 build: pass |
| Same golden test, native arm64 build | different hashes: macOS libm (powf, sin, cos, log10) differs between arm64 and x86-64 |
| Plugin output against a direct device run (`tests/host_smoke.cpp`), random block sizes and parameter moves | 0 samples differ |
| Planted 1-ulp bug | caught by the function and device checks |

The reference was the original x86-64 image driven by a fake host, not Reason itself.

## Not verified

- No comparison with audio rendered by Reason.
- The arm64 plugin against the original's arm64 build (no arm64 oracle yet). The arm64 build
  uses no fused multiply-adds and imports the same four libm functions, so it is expected to
  match, but this is not measured.
- Dry/Wet's default (0.5 here) and the original's value display units: the original stores
  neither in patches.
- What Reason sends on the first batch after creating a device (modelled as every property
  changing) and what an unconnected input buffer holds (modelled as whatever the host passes).
- Not opened in a DAW yet.

## Build

```sh
./build-mac.sh
```

builds the arm64 bundle, runs the host test, installs to `~/Library/Audio/Plug-Ins/CLAP` and
checks the installed bundle. Tests: configure with `-DBUILD_TESTING=ON`, then `ctest`.
