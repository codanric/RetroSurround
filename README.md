# RetroSurround

A VST3 plugin that decodes matrix-encoded stereo (Dolby Surround / Pro
Logic / Pro Logic II style content) into discrete multichannel audio in
real time — for using an always-on surround decoder system-wide (e.g.
via Equalizer APO) rather than only inside a DAW.

Built on [FreeSurround](https://github.com/rndusr/freesurround-fork)
(originally `foo_dsp_fsurround`), a GPL v2-licensed decoder by Christian
Kothe. See [CREDITS.md](CREDITS.md) for full attribution.

## Features

- Real-time stereo-to-multichannel decoding: 2.1 through 7.1, several
  channel layouts per speaker count (see BUILD_INSTRUCTIONS.md)
- Dedicated time-domain bass crossover, replacing FreeSurround's
  original FFT-bin-based bass redirection
- Casio F91W-inspired GUI: amber-on-black, self-drawn parameter
  displays with min/max descriptive labels, one-click Reset

## Building

See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md).

## License

GPL v2 — see [LICENSE](LICENSE). This project directly incorporates
Christian Kothe's GPL-licensed FreeSurround code, so the combined work
is subject to GPL v2's terms: if you distribute this plugin (including
a compiled build) to others, the GPL requires making the corresponding
source available to them under GPL-compatible terms. Also bundles KISS
FFT (BSD-3-Clause) by Mark Borgerding — see `Source/COPYING`.
