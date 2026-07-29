# RetroSurround VST3 — Build Instructions

## Part 1 — Get JUCE + Projucer

1. https://juce.com/get-juce/ → download JUCE (includes `Projucer.exe`).
2. Extract somewhere with **no spaces in the path** — e.g. `C:\dev\JUCE\`.
3. Run `Projucer.exe`. First run may ask you to confirm your JUCE
   installation path — point it at the folder from step 2.

## Part 2 — Open and build

1. Keep this repo's folder structure as-is — `RetroSurround.jucer` needs
   to stay alongside `Source/`.
2. In Projucer: **File → Open...** → `RetroSurround.jucer`. Every source
   file and module is already configured; nothing to fill in.
3. Click **Visual Studio 2022** in the exporter list → **Save and Open
   in IDE**.
4. In Visual Studio, build **Release**.

Built VST3 lands at:
```
Builds/VisualStudio2022/x64/Release/VST3/RetroSurround.vst3
```
Copy to `C:\Program Files\Common Files\VST3\` and rescan in your host.

If you get a linker error about a missing `.lib` file after pulling a
newer commit, delete the `Builds/` folder and re-export from Projucer
rather than rebuilding stale generated project files.

## Latency

Algorithmic latency is ~10.7ms at 48kHz (512 samples, half the 1024-
sample analysis window), scaling proportionally at other sample rates.
Bass management uses a dedicated time-domain crossover, decoupled from
FreeSurround's own internal FFT-bin-based bass redirection (which is
unreliable at small window sizes) — but the *spatial* decode itself
still needs the full 1024-sample window for correctness; an earlier
attempt to shrink it introduced real, measurable harmonic distortion in
bass-range content, confirmed via direct FFT analysis and reverted.

## Output channel configurations

Selectable via a dropdown in the plugin's own GUI (an active request to
the host, not just a passive wait — host support for honoring this
varies):

- Stereo + LFE (2.1)
- 4.1 (front + rear, no center)
- 5.1 Standard (front + rear), with a separate "Legacy Transform" toggle
  — same speaker positions, older internal FreeSurround decoding
  algorithm; the older algorithm doesn't support the Focus control
- 6.1 (side + back center)
- 7.1, in three variants with genuinely different physical channel
  positions: Standard (side + back), Panorama (5-speaker front arc +
  side, no back speakers at all), Tri-Center (same front arc + back
  instead of side)

## GUI

Casio F91W-inspired theme (black body, amber LCD-style readouts, blocky
bargraph sliders and switches). Each parameter shows the value it's
actually set to (self-drawn/editable, not JUCE's built-in slider text
box) plus min/max descriptive labels (e.g. "MONO" → "WIDE") so the
number has context. A **Reset** button restores every parameter, the
channel setup, and the Legacy Transform toggle to their defaults in one
click.
