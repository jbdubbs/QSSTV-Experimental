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
`--noise-hz Hz` (with `--ideal`), `--clock-err frac`, `--tshift samples`, `--out prefix`, `--wav file`.

The `--wav` file has no leader or VIS code (the test only sends the picture lines), so it is for other decoders'
line-sync paths. For a live check of VIS detection and sync in the app, transmit with QSSTV itself (sound output to
file) and receive that file.

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
alongside the standard one and the fast modes (PD120, PD120S, PD120W, JB60) read it when the RX "Wide Video Filter"
box is ticked (default off). Real chain, clean, `--suite`:

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
