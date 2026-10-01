# Mode loopback test (JB60, PD120 control)

TX -> channel -> RX for the QSSTV-engine modes, without a GUI or sound card. The real `modeJB60` / `modePD` /
`modebase` / `sstvparam` classes transmit and receive. In between, by default, is the **real receive front end**:

    frequency track -> 16 bit audio -> dsp/downsamplefilter (48k -> 12k)
                    -> dsp/filters videoFilter (181 tap complex FIR + atan2 discriminator) -> mode->process()

That video filter (about +/-600 Hz) is what limits horizontal sharpness: an edge takes ~0.85 ms (~4.4 slots of
190 us) to rise. A mode that puts more than one pixel in a slot is blurred proportionally. The first version of
this test skipped the filter (`--ideal` still does) and could not see it, which is how the quincunx JB60 (commit
aa21f70) looked fine in the test and blurry on the air. JB60 now sends one luma slot per pixel for that reason.

## Build and run

    ./build.sh
    QT_QPA_PLATFORM=offscreen ./loopback jb --suite          # the table below (use `pd` for the control)
    QT_QPA_PLATFORM=offscreen ./loopback jb --image card --snr 25 --ssb --out /tmp/jb   # writes /tmp/jb_src.png, /tmp/jb_rx.png
    QT_QPA_PLATFORM=offscreen ./loopback jb --image card --wav /tmp/jb_card.wav          # 48 kHz mono audio of the image lines

Options: `--image 0|1|2|card|vedge320|vedge321|hedge248|hedge249|grath|gratv|gratd|file.png` (a file is scaled to
640x496), `--suite`, `--ideal`, `--fir wide|narrow` (the RX "Wide Video Filter", default narrow), `--snr dB` (white noise, dB in 2.7 kHz), `--ssb` (300-2700 Hz, zero phase),
`--noise-hz Hz` (with `--ideal`), `--clock-err frac`, `--tshift samples`, `--out prefix`, `--wav file`
(`--vis` adds the real transmitter preamble and VIS code so the application can detect the mode, `--count N` puts N
pictures in the file). `./loopback --compare a.png b.png` prints PSNR and SSIM of two pictures.

Without `--vis` the `--wav` file holds only the picture lines (no leader or VIS code), which is enough for decoders that
lock on sync alone. With `--vis` it is a complete recording that `qsstv --batch` decodes end to end (see
`tests/filedecode/run.sh`).

`images/card.png` is a fixed text-and-graphics card (colour-on-colour text from 52 px down to 10 px, 1 px lines,
colour bars, a diagonal). The "text region" metrics use its rows 120-300.

## What is and is not modelled

- Modelled: audio, the real downsampler, the real video filter, optional AWGN and SSB band limit.
- Not modelled: the sync detector. The app aligns lines from sync pulses; here the fixed delay of the video path
  (about 112 samples: 90 for the FIR, the rest downsampler and discriminator) is measured once by sending a step
  through the same chain and the demod track is shifted back by it. `--tshift` moves the sampling point;
  +/-1.5 samples costs about 0.5 dB or less of luma PSNR for both PD120 and JB60 on the card.
- Not modelled: real radio paths (fading, AGC, clock drift beyond `--clock-err`), and slant correction.

## Reference numbers

Real chain, clean, `--suite` (recorded 2026-09-28). "quincunx" is the old JB60 (aa21f70), kept for comparison.

| | PD120 | JB60 quincunx | JB60 (luma + difference) |
|---|---|---|---|
| Edge 10-90% rise, x (px) | 4.36 | 8.46 | 4.30 |
| Edge 10-90% rise, y (px) | 0.80 | 1.37 | 0.80 |
| MTF horizontal, period 16 / 12 / 8 px | 0.87 / 0.73 / 0.36 | 0.37 / 0.08 / 0.00 | 0.88 / 0.74 / 0.38 |
| MTF vertical, period 8 / 5 / 4 px | 0.99 / 0.99 / 1.01 | 0.90 / 0.83 / 0.63 | 0.99 / 1.00 / 1.00 |
| Card luma PSNR (dB) | 21.55 | 18.56 | 21.48 |
| Card SSIM, text region / full | 0.886 / 0.900 | 0.823 / 0.801 | 0.869 / 0.887 |
| Card gradient kept, text region | 0.60 | 0.40 | 0.52 |

Ideal (baseband) channel, same suite, for comparison: PD120 x edge 1.87 px, card luma 23.91 dB; quincunx JB60 x edge
2.74 px, card luma 20.27 dB.

Card with noise and SSB band limit (luma PSNR dB / SSIM full):

| | PD120 | JB60 quincunx | JB60 (luma + difference) |
|---|---|---|---|
| 25 dB | 21.37 / 0.831 | 18.90 / 0.766 | 21.28 / 0.822 |
| 15 dB | 20.78 / 0.596 | 18.73 / 0.573 | 20.63 / 0.592 |

Acceptance gate used when JB60 was redesigned: through the real filter, luma PSNR within 1 dB of PD120 and text SSIM
within 0.05 (on the card, and on a photo with text overlay), and at least 2 dB better than the quincunx JB60.

## JB60 design notes (from the parameter sweep)

- Per line pair: L 640 + D 144 + Cr 224 + Cb 176 = 1184 slots (61.27 s), same line time as the quincunx JB60.
- Horizontal MTF equals PD120's exactly (L is one slot per pixel); vertical MTF equals PD120's (D carries the
  vertical detail). What JB60 gives up relative to PD120 is chroma resolution: about 2.9 px (Cr) and 3.6 px (Cb) per
  slot against PD120's 1 px, so saturated colour edges bleed more than PD120's.
- D is cheap: with 64 slots luma PSNR is only ~0.1 dB below 192 slots. Slots moved from D to chroma raise colour PSNR
  a little (card all-channel 19.05 -> 19.16 dB) for ~0.03 dB luma. The split is a flat optimum; 144/224/176 is a
  compromise.
- Companding exponent 0.5-0.7 all score the same; linear (1.0) is clearly worse in noise (card SSIM 0.82 -> 0.79 at 25 dB).
- Averaging two demod samples per slot beats taking one by 0.06-0.13 dB.

## Wide video filter (`--fir wide`)

`dsp/filters.cpp` `videoFilter(maxLength, true)` is the same demodulator with a Kaiser windowed-sinc low pass at
+/-1000 Hz instead of QSSTV's +/-600 Hz filter (same 181 taps, so the same 90 sample group delay). The app runs it
alongside the standard one, and the RX "Wide Video Filter" checkbox (default off) controls two different things
depending on mode: PD120/PD120S/PD120W read it for the *whole picture* (`--fir wide` here reproduces that), while
JB60 reads it for *Cr/Cb only* (`--chroma-wide` here; see the "Chroma-only wide filter for JB60" section below) --
L/D always stay on the narrow filter for JB60. What follows in this section is the whole-picture measurement
(`--fir wide`), i.e. what PD120 still does and what JB60 used to do before the per-segment split. Real chain,
clean, `--suite`:

| | PD120 narrow -> wide | JB60 narrow -> wide |
|---|---|---|
| Edge 10-90% rise, x (px) | 4.36 -> 2.60 | 4.30 -> 2.53 |
| Card luma PSNR (dB) | 21.55 -> 22.46 | 21.48 -> 22.63 |
| Card SSIM, text region / full | 0.886 -> 0.907 / 0.900 -> 0.919 | 0.869 -> 0.896 / 0.887 -> 0.912 |

The price is noise. JB60 on the card with an SSB-filtered channel, SSIM full narrow -> wide (luma PSNR is better
down to about 20 dB, but the noise texture in flat areas costs structure well before that):

| SNR in 2.7 kHz | 50 | 40 | 35 | 30 | 25 | 20 | 15 | 10 |
|---|---|---|---|---|---|---|---|---|
| SSIM full | .886 -> .910 | .884 -> .902 | .879 -> .883 | .861 -> .843 | .822 -> .750 | .735 -> .600 | .592 -> .441 | .435 -> .316 |

On a photo with a text overlay the crossover is a little lower (at 30 dB: PSNR 24.39 -> 27.02 dB, SSIM 0.879 -> 0.884).
So the wide filter is a win for clean paths (cable, local VHF/UHF FM, strong signals, about 30-35 dB and better) and
a loss for typical noisy HF (15-25 dB). CPU cost of the second demodulator: about 0.2% of one core.

## Chroma-only wide filter for JB60 (`--chroma-wide`)

The whole-picture toggle above sharpens L along with Cr/Cb, which is also where nearly all its noise cost comes
from. JB60 subsamples chroma (2.9-3.6 px/slot) far more than luma (1 px/slot), so its Cr/Cb pay proportionally more
for the standard filter's ~4.4-slot blur than L does. `modeJB60::getPixels()` reads the wide track (`sampleWide`,
fed via `modeBase::setWideDemod()`) instead of the narrow one only when `debugState` is Cr or Cb and the same RX
checkbox is on; L and D never move off the narrow filter regardless. `videoFilterWideSuits()` no longer lists JB60,
so the old whole-picture path (`modeDemodPtr()`/`rxUseWideFilter`) always gives JB60 the narrow buffer.

Real chain, clean, card image (2026-09-28; recorded before TX chroma pre-emphasis below existed and was made
unconditional on 2026-09-29 -- current runs will read a bit higher on both sides of this table, since
pre-emphasis now stacks underneath whichever RX filter is chosen, but the off-vs-on *shape* is unaffected, this
section's own numbers were never re-measured):

| | chroma-wide off | chroma-wide on |
|---|---|---|
| Card PSNR: R / G / B / luma / all (dB) | 18.74 / 19.42 / 18.68 / 21.04 / 18.93 | 19.14 / 19.61 / 19.35 / **21.05** / 19.36 |
| Card SSIM full / text-region (luma only) | 0.879 / 0.860 | 0.879 / 0.860 |
| Pure chroma edge (`cedge320/321`) 10-90% rise, B (px) | 15.40 / 15.42 | 10.38 / 10.88 |

Luma PSNR and SSIM are unchanged (as designed -- SSIM is a luma-only metric here, so it can't see a chroma-only
change at all); the whole-picture toggle's SSIM cost never applies. Confirmed no group-delay/timing shift from
using a different filter for chroma only: the 50%-crossing point of a real edge moved by at most 0.26 px between
on and off (measured on `medge321`'s R and B channels), consistent with the two filters' identical 181-tap/90-sample
group delay -- no `bp`-style correction needed.

Noise crossover, card, SSB-filtered channel (all-channel PSNR dB, off -> on):

| SNR in 2.7 kHz | 40 | 30 | 25 | 20 | 15 | 10 |
|---|---|---|---|---|---|---|
| PSNR all channels | 18.85 -> 19.24 | 18.82 -> 19.16 | 18.75 -> 18.99 | 18.54 -> 18.50 | 17.95 -> 17.29 | 16.62 -> 15.00 |
| SSIM full (luma) | .873 -> .873 | .853 -> .853 | .815 -> .812 | .728 -> .723 | .585 -> .577 | .429 -> .422 |

The crossover (roughly 20-25 dB) isn't dramatically lower than the whole-picture toggle's own PSNR crossover
(~20 dB) -- the original hypothesis that chroma's noise tolerance would push it much lower wasn't clearly borne
out. What *is* better than the whole-picture toggle: below the crossover, only chroma gets noisier, never L, so
the worst case at poor SNR is strictly less damaging than the old all-or-nothing choice (which paid the same noise
price on text and edge sharpness too). Same default-off recommendation as the whole-picture toggle.

Caveat found while measuring: the mixed luma+colour edge test (`medge320/321`)'s automated B-channel edge-rise
number is unreliable at some sample alignments -- it can jump by >10x from a real filter overshoot/ringing pattern
crossing the metric's naive 10-90% thresholds in a non-monotonic way, even when the underlying pixels and PSNR both
show an improvement (see the `jb60-color-smear-ideas` memory, TX idea 5 section, for the fuller diagnosis of this
same metric issue). Card PSNR/SSIM and the pure `cedge` bandwidth test are the trustworthy numbers here; treat
`medge`'s B rise as directional at best, and double check against PSNR and a direct pixel/crossing-position look
before trusting a large swing in it.

## TX chroma pre-emphasis for JB60 (unconditional -- part of the spec)

The two filters above are RX-side attempts at the same problem: the video demod filter's ~600 Hz (narrow) /
~1000 Hz (wide) lowpass smears a 190 us slot over several pixels. This is a TX-side attempt instead: JB60's Cr/Cb
slot sequence is boosted *before* modulation, so every receiver gets a slightly sharper picture with no RX-side
change at all. `modeJB60::getLine()` applies it to `redArrayPtr`/`blueArrayPtr` unconditionally, right after
`downsampleChroma()` fills them; L/D are untouched. **This shipped first as an opt-in TX checkbox (default off,
2026-09-29), then was hardcoded on and the checkbox removed once the extended SNR sweep below found the risk of
leaving it optional wasn't worth the UI cost** -- see that decision at the end of this section. There is no way
to turn it off; a "before" comparison below means "the plain, unmodified 2-tap-linear-downsample byte sequence,"
not a currently-reachable code path.

Kernel: a 5-tap, zero-phase, unity-DC-gain FIR at the slot rate (~5.26 kHz for JB60's ~190 us slot):
`h = [-0.1357, -0.2133, 1.698, -0.2133, -0.1357]` (center, ±1, ±2 slots). Designed as a regularized inverse of the
*narrow* video filter's measured magnitude response (`Ginv(f)=min(1/A(f), Gmax)`, `Gmax=6dB`, `fc≈599Hz` -- the
narrow filter's own -6dB point), realized as identity plus a DC-leak-corrected `firwin2` fit so `sum(h)=1.0`
exactly. Not designed against the wide filter (see spot check below). A short symmetric FIR can only realize a
*monotonic* high-shelf, not the originally-intended "boost then roll back to unity" bump -- verified: even at 13
taps, a frequency-sampling fit of that bump shape only reached ~55-60% of target gain and never returned to 0dB.
So the shipped shape is cap-and-hold (flat at `Gmax` out to the slot Nyquist, not rolled back above the filter's
near-null), which is why the `Gmax` sweep below matters more than the design formula alone -- see the
`jb60-color-smear-ideas` memory, TX idea 6, for the sweep that picked `Gmax`/tap-count and the sign/overshoot
analysis. `clampByte()` (already used elsewhere in the file) contains any overshoot at real transitions.

Real chain, card image (2026-09-29), without vs with (measured while it was still an opt-in checkbox; "with" is
what every current build now always does):

| | without | with (now the only option) |
|---|---|---|
| Card PSNR: R / G / B / luma / all, clean (dB) | 18.74 / 19.42 / 18.68 / 21.04 / **18.93** | 18.77 / 19.44 / 18.79 / 21.05 / **18.99** |
| Card PSNR: all, 25 dB SSB (dB) | 18.75 | 18.80 |
| Card PSNR: all, 15 dB SSB (dB) | 17.95 | 18.00 |
| Pure chroma edge (`cedge320/321`) 10-90% rise, B (px) | 15.40 / 15.42 | 14.75 / 14.79 |

On a real photo (not synthetic -- a saturated-colour cat photo) the win is bigger, not smaller: all-channel PSNR
23.95->24.26 dB clean, 23.49->23.75 dB at 25 dB SSB, 21.73->21.90 dB at 15 dB SSB, **and it's still a (smaller)
win at 5 dB SSB: 15.43->15.47 dB** -- no crossover found from 5-40 dB SNR on either test image, unlike the two
RX-side filters above (~20-25 dB crossover) or RX idea 4's expected one. This matches the textbook pre-emphasis
argument: boosting happens *before* the channel adds noise, so it doesn't trade sharpness for noise robustness
the way RX-side sharpening does.

`Gmax` sweep (5-tap, card, real chain) found a broad flat PSNR optimum at 5-7dB, turning over by 8-9dB and
**regressing below baseline by 12dB** (18.93->18.84dB clean) -- while the naive `cedge` B-rise metric kept
improving monotonically the whole way to 12dB. Same "sharper single-edge metric, worse real image" trap already
flagged for TX idea 5 and RX idea 3, live here too: trust card PSNR/SSIM to pick `Gmax`, never a lone edge-rise
number. Kernel-length check at Gmax=6dB: 3 taps -> +0.03dB clean, 5 taps -> +0.06dB (about double), 7 taps ->
+0.08dB (smaller further gain) -- 5 taps shipped as the simplicity/benefit balance.

Group-delay: `calibrateDelay()` (used by the two RX-side filters above) is **structurally blind** to this change,
since its synthetic step bypasses `modeJB60::getLine()` entirely -- confirmed identical "video-path delay" across
every `Gmax` tested. The real check is a direct 50%-crossing measurement on a transmitted `cedge320/321` edge
(`metrics::edgeCrossChannel()`, printed by `--image cedge320`/`cedge321`'s report line): B channel moved by
**-0.08px** (cedge320) and **+0.01px** (cedge321) with pre-emphasis on, both well inside the wide-filter section's
own accepted ≤0.26px precedent above -- no `bp`-style RX timing correction needed.

Wide-filter spot check (designed against narrow only, per the jb60-color-smear-ideas memory's scoping decision):
combined with `--chroma-wide`, all-channel PSNR 19.36->19.40dB; combined with `--fir wide` (the legacy
whole-picture path), 19.57->19.62dB. No regression either way.

Regression canary (`--suite`): MTF (grating images, all grayscale so Cr/Cb sit at the flat 127.5 midpoint --
exactly where a unity-DC-gain kernel is a no-op) and edge x/y rise (also grayscale) are bit-identical with vs
without, confirming L/D are untouched. Card's own "luma PSNR" (computed from the *reconstructed* R/G/B, not the
internal Y slot values) wobbles by +0.01dB (21.04->21.05) -- this is a byte-rounding artifact of the RGB
reconstruction (`R=Y+1.4(Cr-127.5)`, `B=Y+1.78(Cb-127.5)`, `G` solved from `Y`), not a real L/D leak; two orders of
magnitude below the intended Cr/Cb win and confirmed harmless by the grayscale-image tests above.

### Extended SNR sweep and the decision to hardcode it (2026-09-29)

The numbers above only went down to 5 dB SNR, so before deciding whether an opt-in checkbox was the right
long-term shape, the sweep was extended to -10 dB (card and the photo, all-channel PSNR, without -> with):

| SNR | 10 | 5 | 0 | -2 | -4 | -6 | -8 | -10 |
|---|---|---|---|---|---|---|---|---|
| card | +0.03 | +0.02 | +0.01 | 0.00 | 0.00 | +0.02 | +0.02 | 0.00 |
| photo | +0.10 | +0.04 | +0.03 | +0.02 | +0.01 | **-0.01** | **-0.02** | **-0.01** |

A real crossover exists, but only on the photo, only below about -4 to -6 dB, and it's tiny (-0.01 to -0.02 dB)
against +0.05 to +0.31 dB everywhere realistic -- smaller than the effect doesn't reproduce on the card image at
all, which points to measurement noise rather than a robust effect. More importantly, SSIM at -6 dB and below is
already 0.02-0.07 on both images -- the picture is unusable static regardless of this setting, so the crossover,
where it exists, doesn't correspond to any real decision a receiving operator would make differently.

This is why it shipped as a checkbox first rather than going straight to unconditional: ideas 2/3 (pure local RX
retuning, reversible, zero effect on anyone else) needed to win at *every* SNR tested before shipping
unconditionally; a TX-side change is heard by every receiver, so the bar for going unconditional was higher, not
lower, and the initial ship was deliberately conservative (opt-in, default off) until this extended sweep existed.
With it in hand -- no crossover in any SNR regime where the picture is actually viewable, on either test image --
the checkbox was removed and pre-emphasis made unconditional, on 2026-09-29. What this sweep still doesn't cover:
non-AWGN channel effects (fading, impulsive QRN, real transceiver TX audio chains) and decode quality on
non-QSSTV receivers (confirmed format-compatible for the Android `robot36` port, but not measured there) -- there
was no realistic way to get that data without on-air use, so it was accepted as residual risk rather than a
blocker.

## RX idea 4: regularized chroma deconvolution for JB60 (`--chroma-deconv`)

The three sections above either widen the RX filter (at a noise cost) or boost chroma before
modulation (no RX cost, but only helps pictures this app's own TX actually sends). This is the
last RX-only idea from the `jb60-color-smear-ideas` brainstorm: a short, opt-in, *regularized*
inverse filter applied to Cr/Cb right after RX capture (`modeJB60::showLine()`, before
`emitPair()`/`upsampleChroma()`), gated by a new "JB60 Chroma Deconvolution" checkbox
(`RX/chromaDeconvolution`, default off, `src/sstv/chromadeconvolution.{h,cpp}`, same
override-for-testing pattern as the wide-filter checkbox). It only ever touches `curCr`/`curCb`
when the Cr/Cb wide-filter track is *not* in use (`!wideVideoFilterEnabled()`); the UI also grays
the checkbox out in that case, since the kernel is matched to the narrow filter only.

**Design-target decision:** TX chroma pre-emphasis above is unconditional, so every picture this
app now transmits already arrives partially corrected. This filter is designed against that
*combined* (pre-emphasis + channel) response, not the plain channel alone -- optimal for pictures
sent by this app (the common case going forward), a known, uncharacterized mismatch for older
recordings or other encoders' output that never had pre-emphasis applied.

### Measuring the combined response (`--dump-slots`)

No committed tool previously measured the slot-domain frequency response directly (TX idea 6's own
measurement was done out of band and never checked in). `loopback --dump-slots cr|cb|both` fixes
that: it runs the real TX -> channel -> RX chain on a purpose-built image and prints the resulting
`modeJB60::lastCr()`/`lastCb()` slot arrays (the last line pair's demodulated, pre-deconvolution
Cr/Cb -- `showLine()` swaps its freshly-`assign()`'d arrays into `prevCr`/`prevCb` before
returning, so these hold exactly that pair's capture right after any `process()` call).

The test image is a **Y-luma-held-constant step**: flat `(128,128,128)` background, the last line
pair's right half set to `(255,87,0)` (R and B pushed hard in opposite directions -- Cr and Cb
both get a large excitation -- while G is chosen so luma stays ~128 on both sides, within 0.2
counts). Holding luma constant collapses `downsampleChroma()`'s luma-guided weights to a flat box
average across the transition (no coincident luma edge to guide on, so every footprint weight is
uniform), turning the whole TX+channel+RX path into a plain LTI system for this measurement -- and
because 224 and 176 both divide 640 evenly, the step lands exactly on a footprint boundary, so the
*known* (box-average-only, no pre-emphasis) input signal is a perfect single-sample impulse. That
makes the *measured* output's discrete derivative directly proportional to the combined system's
impulse response, with far better SNR than exciting a literal one-column impulse in the image (the
same script tried that first: peak deviation ~9 counts out of 255, versus ~90 counts for the step,
about 10x better dynamic range against the demodulator's own rounding noise).

Two real artifacts had to be found and worked around, both segment-boundary effects, not bugs: (1)
the very end of Cb's array droops because Cb is immediately followed by `fp`/sync (a fixed, very
different tone) in transmission time, and the channel's finite-length smearing bleeds that boundary
backward into Cb's last ~10 slots; (2) the very start of Cb's array is contaminated the same way
from the *other* side, because Cr and Cb are each an independent full-640-column box average of
the same row resampled to their own slot count -- adjacent in transmission time (segment order
`L,D,Cr,Cb`), not in column space -- so Cr ending at the high plateau (its last slots cover columns
near 640, past the step) walks straight into Cb starting at the low plateau (its first slots cover
columns near 0, before the step), a real ~90-count content discontinuity at that boundary. Cr has a
smaller version of the same tail artifact from the same Cr->Cb boundary. Both are masked (replaced
with the nearby true plateau value) before differencing; the actual transition of interest sits
comfortably inside each array, well clear of either masked region.

### Wiener design

`Hinv(f) = |H(f)| / (|H(f)|^2 + K)`, phase forced to zero (measured phase is small, <5 degrees, up
to about 800 Hz, then noise-dominated past the filter's own -15dB@804Hz/null~1150Hz -- consistent
with the narrow filter's already-documented shape). Two refinements over a naive flat-K fit, both
found necessary by inspecting the first candidates' taps and PSNR, not assumed up front:

- **DC pinned to an exact 1.0**, not the Wiener-shrunk value. The measured combined response's own
  DC gain is already ~1.0 (0.991 Cr, 1.003 Cb) -- flat, already-correct colour passes through the
  real chain essentially unchanged, so there's no regularization risk at f=0 and a flat `K` was
  unnecessarily shrinking it anyway (down to 0.91 at `K=0.1`), which *desaturates* flat regions,
  the opposite of the goal. The fitted FIR enforces this exactly too (an equality-constrained
  least-squares projection onto `h0 + 2*sum(h_t) = 1`), not just the continuous target curve.
- **Target held flat past 1100 Hz** rather than fit all the way to the 2631 Hz slot Nyquist, since
  the measurement itself is noise-dominated up there (erratic phase, non-monotonic magnitude) --
  fitting that region would inject measurement noise into the filter, not signal.

Fit to a short symmetric (zero group delay) FIR by weighted least squares over the cosine basis
(`Hfir(f) = h0 + 2*sum h_t*cos(2*pi*f*t/fs)`, `fs≈5262 Hz`), DC bin weighted heavily to make the
equality projection well conditioned.

Tap-count sweep (`K=0.1`, card and a hard-edged colour-bars/circles image standing in for a real
photo -- no photo is committed to this repo; TX idea 6's own sweep used one that lives only on the
machine it was measured on):

| L (taps) | 3 | 5 | 7 | 9 |
|---|---|---|---|---|
| Card PSNR, all (dB) | 18.99 | 19.01 | 19.03 | 19.04 |
| Hard-edge image PSNR, all (dB) | 13.54 | 13.54 | 13.55 | 13.56 |

Diminishing returns past 7, matching TX idea 6's own tap-count finding. `K` sweep at 7 taps (same
two images, clean):

| K | 0.5 | 0.3 | 0.2 | 0.15 | 0.1 | 0.07 | 0.05 | 0.03 | 0.02 | 0.01 |
|---|---|---|---|---|---|---|---|---|---|---|
| Card PSNR, all (dB) | 18.86 | 18.92 | 18.97 | 19.00 | 19.03 | **19.05** | **19.06** | 19.04 | 18.99 | 18.87 |
| Hard-edge PSNR, all (dB) | 13.51 | 13.53 | 13.54 | 13.55 | 13.55 | 13.55 | 13.55 | 13.54 | 13.52 | 13.48 |

A broad flat optimum at 0.05-0.1, regressing below the off baseline (18.99 clean, from the TX
pre-emphasis section's own current-code numbers) outside about 0.02-0.5 -- the same "broad
optimum, regression at both
extremes" shape TX idea 6's `Gmax` sweep found, for the same reason (too little correction leaves
gain on the table; too much starts amplifying error/noise faster than it restores signal).
**Shipped: 7 taps, `K=0.07`** -- close to the 0.05 peak (0.01 dB difference on the card) but with a
smaller centre tap (1.19 vs 1.49 at K=0.05), preferred for slightly less noise-amplification risk
at equal clean-signal gain. Taps: `[1.186478, 0.040282, -0.025199, -0.108322]` (centre, ±1, ±2, ±3
slots).

### Validation

Regression canary (`--suite`): luma PSNR, all three MTF curves, and x/y edge rise (all grayscale,
Cr/Cb pinned at the flat 127.5 midpoint) bit-identical with vs without -- confirms no L/D leak.
Pure chroma edge (`cedge320/321`) B rise narrowed slightly (14.75/14.79px on -> 14.04/14.19px), a
small genuine sharpening, consistent with whole-card PSNR also improving (not the "sharper metric,
worse image" trap flagged repeatedly elsewhere in this file). Group delay
(`metrics::edgeCrossChannel()` on `cedge320/321`'s B channel): -0.019px / -0.031px, both well
inside the ≤0.08px bar used for TX idea 6 -- no RX timing correction needed, as expected for a
symmetric (zero group delay) FIR. Wide-filter interaction: `--chroma-wide` alone and
`--chroma-wide --chroma-deconv` together give identical PSNR (19.40 dB both) -- the code-level
guard is working as designed.

`medge320`'s automated B-channel rise metric jumped from 0.91px to 15.01px on -> off -- this is the
same known-unreliable-metric artifact documented for the chroma-wide-filter section above and TX
idea 5 (a real filter ringing/overshoot pattern crossing the naive 10-90% threshold algorithm
non-monotonically), not evidence of real damage: whole-card and `cedge` PSNR both improved at the
same settings. Treat any lone `medge` swing as unreliable; trust card/`cedge` PSNR first.

SNR sweep (card and the hard-edge image, `--ssb`, all-channel PSNR, without -> with):

| SNR (dB) | 40 | 30 | 25 | 24 | 23 | 22 | 21 | 20 | 15 | 10 | 5 | 0 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Card | 18.90->18.96 | 18.87->18.92 | 18.80->18.83 | 18.77->18.79 | 18.74->18.75 | 18.70->18.69 | 18.65->18.62 | 18.59->18.54 | 18.00->17.79 | 16.65->16.20 | 14.19->13.50 | 10.35->9.82 |
| Hard-edge | 13.47->13.49 | 13.47->13.48 | 13.45->13.45 | -- | -- | 13.42->13.41 | -- | 13.38->13.37 | 13.21->13.14 | 12.73->12.53 | 11.48->11.06 | 8.84->8.45 |

A real crossover, around **22-25 dB SNR** on both images -- small, growing losses below it (-0.7dB
by 5dB on the card), small, consistent gains above (+0.03 to +0.06dB). Unlike TX idea 6, this *is*
an RX-side filter and so pays the noise-amplification tax its own original risk framing predicted;
the crossover sits close to the chroma-only wide-filter section's own ~20-25dB finding above, which
makes sense (same mechanism: an RX-side operation on the already-noisy demodulated Cr/Cb, not a
before-the-channel TX boost). **Shipped default: off**, same reasoning as the wide-filter checkbox
-- the RX tooltip says "leave it off for typical HF" and names ~25 dB.

## The application against this harness

Decoding these recordings with the real application (`qsstv --batch`, real VIS detection and sync) gives pictures that
match the harness output at a sampling shift of **+2 to +2.5 samples** (12 kHz): the application's video demodulator and
its sync detector are separate filter chains with different group delays, so a receiver that times pixels purely from
the detected sync position samples about 2.5 samples (208 us, 1.1 of a 190 us slot) later than the harness's own
calibrated-delay reference. The cost on the test card (luma PSNR, harness at shift 0 against shift +2):

| | shift 0 | shift +2 (before the fix below) |
|---|---|---|
| JB60, standard filter | 21.48 | 20.87 |
| JB60, wide filter | 22.63 | 21.01 |
| PD120, standard filter | 21.55 | 21.28 |

**Fixed for JB60** (`src/sstv/sstvparam.cpp`): its RX back porch (`bp`) is trimmed by 2.5 samples (0.00208 s -> 0.00187 s)
relative to its TX porch (`bpt`, left at 0.00208 s, so nothing about the transmitted signal changes). That shifts every
pixel-sampling position in the received line earlier by the same amount, which is exactly the compensation needed.
Verified by comparing the real application's decode against this harness's own ideal-timing (shift 0) reference,
sweeping candidate porch values in steps of one 12 kHz sample: match quality rises from 31.7 dB (broken) to a flat
peak of 40-42 dB around -2.5 to -3 samples. The `tests/filedecode` "receive-timing regression" check asserts this
stays above 35 dB. PD120's tables already do the equivalent compensation, on the other side: its transmitter porch is
0.22 ms (2.6 samples) longer than its receiver's, which is why it showed a smaller residual error above. PD120 was
left alone here since it wasn't part of this fix; its own small residual gap (about 0.3 dB) is unchanged.

## Right-edge check (issue #18)

Every single-image run prints `right-edge error`: the worst R/G/B channel and, separately, the luma error of the
row-averaged |rx-src| over the last 8 columns, next to the same two figures for an interior block 40 columns in.
`loopback --edge src.png rx.png` prints the same four numbers (edge channel, edge luma, interior channel, interior
luma) for any decoded picture, e.g. the application's own, and `tests/filedecode/run.sh` asserts the application's
edge channel error stays <= 15.

JB60's last Cb slot sits against the sync pulse (no front porch) and used to decode as a green/yellow strip; the
receiver now replaces the last 5 Cb / 3 Cr slots with the last unaffected one (`showLine()`). Card image, before ->
after: ideal channel 60.5 -> 2.1, real chain with `--chroma-wide --snr 25 --ssb` 68.8 -> 7.5 (interior
26.8), through the application ~60 -> ~9 (interior ~26 with noise). The luma column exists because a second artifact is not fixed yet: the last luma slot sits
against D's mid-level and the last 2-3 columns of a dark picture come out bright (a dark-edged test image reads
about 17 luma counts in the harness and ~34 in the application, against ~1 interior).
