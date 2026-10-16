# RetroSurround VST3 — Build Instructions

## Build a production VST3

Before building, you must install **CMake 3.22 or newer**, **Visual Studio
2022 or Build Tools with the Desktop development with C++ workload**, and a
**Windows 10/11 SDK**. These are required system build tools; the script does
not install them. JUCE can be supplied from an existing source checkout or
downloaded into the ignored build directory with `-BootstrapJuce`.

1. Install CMake from the [official CMake download page](https://cmake.org/download/).
   Choose the Windows x64 installer and enable adding CMake to `PATH`.
2. Install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/community/)
   or [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/).
   In Visual Studio Installer, select the **Desktop development with C++**
   workload. Ensure the MSVC x64/x86 build tools and a Windows 10 or Windows 11
   SDK are selected in the workload's individual components.
3. If no Windows SDK was selected with that workload, reopen Visual Studio
   Installer, choose **Modify → Individual components**, select a **Windows 10
   SDK** or **Windows 11 SDK**, and apply the change.
4. Open a new PowerShell window after installation so the updated `PATH` is
   available. The build script detects CMake, Visual Studio C++ tools, and the
   SDK and reports a clear error if one is missing.

The script configures an x64 build and explicitly compiles the **Release**
target.

```powershell
.\Build-Release.ps1 -JucePath C:\path\to\JUCE -Clean #replace with JUCE real path
```

The same build can be started from Command Prompt or Explorer with the batch
launcher. Pass the path to your JUCE checkout, or let the script bootstrap it:

```bat
Build-Release.bat -JucePath "C:\path\to\JUCE" -Clean
Build-Release.bat -BootstrapJuce -Clean
```

The batch file forwards its arguments to PowerShell and pauses after a failed
build so the error remains visible.

If JUCE is not already available, the optional isolated bootstrap is:

```powershell
.\Build-Release.ps1 -BootstrapJuce -Clean
```

This pins JUCE to 8.0.15 under `build-release/_deps/JUCE/`; JUCE bootstrap
does not install the required system build tools. To use nonstandard tool
locations, pass `-CMakePath` and `-VisualStudioGenerator`, set
`JUCE_PATH`, or pass `-JucePath`. Supported generator examples are
`Visual Studio 17 2022` and `Visual Studio 16 2019`. The script prefers JUCE
8.0.15 when it is present beside the repository; pass
`-JucePath` to select another checkout explicitly.

Build files and downloaded JUCE sources stay under the ignored
`build-release/` directory. `-OutputDirectory` can select another
repository-local path; `-Clean` removes it before configuration. The script
checks for the x64 VST3 module before reporting success and never selects
Debug.

Built VST3 lands at:
```
build-release/RetroSurround_artefacts/Release/VST3/RetroSurround.vst3
```
Copy to `C:\Program Files\Common Files\VST3\` and rescan in your host.

If a build fails after source or JUCE changes, rerun with `-Clean` to
regenerate the build tree. Close any host using the output bundle before
cleaning it.

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
