# RPVerb Reconstruction (CLAP)

A CLAP reverb that reproduces the DSP of Rob Papen's RP-Verb RE 1.0.7 Rack Extension. The DSP was
ported by hand from the Rack Extension's x86-64 code, operation by operation, and checked against
the original binary (x86-64 under Rosetta, arm64 natively) and against audio rendered by Reason.
Private project; not affiliated with Rob Papen or Reason Studios.

## Panel

On macOS the plugin has a small GUI laid out along the signal flow. The top row is the main path,
left to right: IN, Ensemble, Distortion, EQ (pre), Pre-Delay, the three reverbs, EQ (post), the
envelope's gain, Dry/Wet, OUT. The bottom row holds the side branches: Early and Late reflections
and the Envelope. Their wires go to whichever point the routing switches select, and the route not
taken is dashed. Above the main row run the dry path and, with Ensemble on "Direct + Reverb", the
ensemble's direct path. The EQ panel moves between its pre and post slots with Post Audio. The
reverb shows which of its three parallel reverbs run (HQ Mode off drops the second), and the
header shows the device's state: running, muted while clearing after a reset, asleep on silent
input, off, or bypass. Drag knobs vertically (Shift for fine), double-click for the default,
scroll to nudge. `rpverb_gui_snapshot <binary> out.png [routed]` renders it to a PNG.

## Controls

Every panel control is a parameter, with the original property tag as its CLAP id:
Power (Off / On / Bypass), Reset, Dry/Wet; Reverb (on, pre-delay, pre-delay disorder, space size,
space disorder, length, damping, volume, diffusion, low-pass, high-pass, Space Type, Colour);
Early and Late reflections; Envelope (mode, mono input, latch, attack, hold, release, amount);
Ensemble; Distortion; EQ; and the back panel's HQ Mode and Reset on Stop. "Side Input" stands
in for plugging a cable into the back-panel side inputs: when on, the plugin's second stereo
input drives the envelope.

Seven remote-control pages put the panel on a host's 8-knob pages (Bitwig's device remote
controls): Main (Dry/Wet, Space Type, Colour, Space Size, Reverb Length, Pre-Delay, Reverb Damping,
Reverb Volume), Reverb, Early, Late & Ensemble, Envelope, Distortion & EQ, and Back Panel (Power,
Side Input, HQ Mode, Reset on Stop, Reset, Dry/Wet). Every parameter is on at least one page.

Host modulation of Dry/Wet, Ensemble Volume, Distortion Amount, Early Volume, Late Volume,
Space Size, Reverb Length, Diffusion and Reverb Damping drives the matching CV input, as a CV
cable does in Reason. Other parameters are automatable but not modulatable.

## How it behaves (as the original does)

- The device works in 64-frame batches, so the plugin has 64 frames of latency. Parameter
  changes take effect at the next batch.
- Eleven or more parameter changes in one batch, a sample-rate change and the Reset control
  start a mute-and-clear of about 96 batches (about 140 ms at 44.1 kHz) during which the output,
  dry signal included, fades out and the input is ignored.
- Activation starts a fresh device the way Reason starts one: every property arrives at creation
  and again from the patch, and both resets finish before the first audio, so nothing is lost at
  the start. A host `reset()` presses Reset and likewise finishes the clear at once.
- When the host transport stops, the plugin passes on the burst of changes Reason sends at a
  stop, so Reset on Stop (on by default) clears the reverb then; switched off, the tail rings on.
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
| Whole device against the original's render, outputs and the full 26 MB state each batch | 0 differ: x86-64, 8 seeds x 5,000 batches; arm64, 8 seeds x 5,000 batches |
| Golden test (`tests/golden.cpp`), 2 x 6,000 batches, hashes measured on the original | pass on both: each build against the original's image for the same architecture (their libm differs) |
| The plugin in a host against Reason 14: a noise burst through a new RP-Verb, exported to 24-bit WAV | within 1.25 LSB at 24 bits over the whole 199 s export (residual 106 dB below the signal) |
| Plugin output against a direct device run (`tests/host_smoke.cpp`), random block sizes and parameter moves | 0 samples differ |
| Remote-control pages (`tests/host_smoke.cpp`): ids valid, every parameter reachable | pass |
| GUI opens through the CLAP gui extension and renders (`gui_snapshot` test) | pass |
| Planted 1-ulp bug | caught by the function and device checks |

The function and device checks drive the original binary with a fake host; the Reason row is the
real host. Reason renders an export from a fresh device, and a second export matched the first
bit for bit.

## Not verified

- Only the default patch was rendered through Reason; other settings rely on the device checks.
- What Reason sends at a transport start, a loop or a jump of the song position (modelled as
  nothing), and what an unconnected input buffer holds (modelled as whatever the host passes).
- The original's value display units (the plugin shows percent).
- Not opened in a DAW yet: GUI gestures and automation recording in a host are untested.

## Build

```sh
./build-mac.sh
```

builds the arm64 bundle, runs the host test, installs to `~/Library/Audio/Plug-Ins/CLAP` and
checks the installed bundle. Tests: configure with `-DBUILD_TESTING=ON`, then `ctest`.
