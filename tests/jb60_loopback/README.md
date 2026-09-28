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
640x496), `--suite`, `--ideal`, `--snr dB` (white noise, dB in 2.7 kHz), `--ssb` (300-2700 Hz, zero phase),
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
  +/-1.5 samples costs about 0.5 dB or less of luma PSNR on the card.
- Not modelled: real radio paths (fading, AGC, clock drift beyond `--clock-err`), and slant correction.

## Reference numbers

Real chain, clean, `--suite` (recorded 2026-09-28). JB60 here is the quincunx layout of commit aa21f70.

| | PD120 | JB60 quincunx |
|---|---|---|
| Edge 10-90% rise, x (px) | 4.36 | 8.46 |
| Edge 10-90% rise, y (px) | 0.80 | 1.37 |
| MTF horizontal, period 16 / 12 / 8 px | 0.87 / 0.73 / 0.36 | 0.37 / 0.08 / 0.00 |
| MTF vertical, period 8 / 5 / 4 px | 0.99 / 0.99 / 1.01 | 0.90 / 0.83 / 0.63 |
| Card luma PSNR (dB) | 21.55 | 18.56 |
| Card SSIM, text region / full | 0.886 / 0.900 | 0.823 / 0.801 |
| Card gradient kept, text region | 0.60 | 0.40 |

Ideal (baseband) channel, same suite, for comparison: PD120 x edge 1.87 px, card luma 23.91 dB; JB60 x edge
2.74 px, card luma 20.27 dB.

Card with noise and SSB band limit (luma PSNR dB / SSIM full):

| | PD120 | JB60 quincunx |
|---|---|---|
| 25 dB | 21.37 / 0.831 | 18.90 / 0.766 |
| 15 dB | 20.78 / 0.596 | 18.73 / 0.573 |

The horizontal MTF of JB60 is one octave worse than PD120's: the quincunx puts two picture pixels in every slot, so
the video filter's ~4.4 slot smear becomes ~8.5 px.
