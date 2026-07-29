# RetroSurround — Credits

**Project lead:** Daniel Ricardo

**Surround decoding engine:** FreeSurround, originally written by Christian
Kothe (2007-2010) as `foo_dsp_fsurround`, a DSP component for foobar2000.
The actual decoding algorithm in this plugin — the amplitude-and-phase
analysis that determines a sound's position from a stereo signal, and the
channel-map lookup tables that turn that position into per-speaker gains
— is Christian Kothe's original work, ported and integrated here with one
bug fix (a memory-management issue in the original code's FFT cleanup)
but otherwise unmodified. Licensed under GPL v2; see `Source/GPL.txt`.

**Engineering, integration, and this rewrite:** Claude (Anthropic) —
architecture, the VST3 plugin wrapper, the Windows-channel-order fix, the
multi-channel-configuration support, the sample-rate-scaling fix, and a
dedicated crossover-based bass management design that replaced
FreeSurround's original FFT-bin-based approach (which was unreliable at
smaller analysis window sizes), all developed and tested in
collaboration with Daniel Ricardo across this project's iterations.

**Also used:** KISS FFT by Mark Borgerding, BSD-3-Clause licensed; see
`Source/COPYING`.

## License note

Because this plugin directly incorporates Christian Kothe's GPL v2
licensed code, the combined work is subject to GPL v2's terms. For your
own personal use, this has no practical effect. If RetroSurround is ever
distributed to others, the GPL requires the combined work to be licensed
under GPL-compatible terms, with source code made available to recipients.
