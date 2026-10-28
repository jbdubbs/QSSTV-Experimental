# Experimental KG-STV

KG-STV is a third receive/transmit mode alongside SSTV and HAMDRM. The native backend is ported from `ploeffler/QSSTV`'s 9.6 integration (commit `3f615db`), adapted to this fork's Qt 6/CMake application. The Experimental version, settings identity, template editor and platform packaging are retained.

## Use

Select **KG-STV (experimental)** in RX and TX. Choose MSK or 4L-FSK to match the peer. RX/TX modulation is synchronized; TX also offers compression level 0..15 and payload FEC.

- Send an image with the normal Start button. KG-STV uses one 320x240 image; templates use this fork's normal editor/rendering. Stitched grids, repeater and camera beacon transmissions are not enabled in this mode.
- Send 1..200 ASCII bytes with **Send Text**; no image is required. The history displays incoming messages and local send/cancel status. A local completion is not a delivery acknowledgment.
- After a completed but incomplete image pass, **Request missing blocks (BSR)** sends missing coordinates. **Send requested blocks from cached image** replies manually from the last completed TX cache. Loading another image does not change that cache. Replies never key the transmitter automatically.
- Stop cancels the native job. Mode changes respect the fork's busy-mode lock. Completed images can be auto-saved in the digital RX directory with a `KGSTV_` prefix; partial images can be saved manually.

The existing audio file picker also works while the KG-STV tab is selected. Headless examples:

```sh
qsstv-experimental --batch --kgstv --kgstv-four-level --out-dir received recording.mp3
qsstv-experimental --listen --kgstv --kgstv-four-level --out-dir received
qsstv-experimental --encode image.png --kgstv --kgstv-four-level --wav-out image.wav
```

The 4L option requires `--kgstv`; omit it for MSK. KG-STV and `--drm` are mutually exclusive. File decoding uses the fork's normal media reader, 48 kHz capture and 12 kHz receive chain. KG-STV headless file receive/encode uses nominal WAV clocks, preserving the saved hardware calibration, and bypasses card enumeration/open and CAT/PTT; live `--listen` retains normal devices. Batch mode saves complete PNGs and received UTF-8 text files in `--out-dir`. Empty/invalid recordings retain the existing nonzero batch exit codes. GUI text entry remains ASCII-only pending further reference evidence. `--encode` normalizes the source image to 320x240; GUI transmission uses the current template-composed preview.

## Implementation and tests

`src/kgstv` contains packet CRC/FEC/whitening, bounded frame streaming, a six-component DCT/Huffman image codec, CPFSK audio, block cache/repair logic and a thread-safe controller. BGR channel order follows the original application. At 12 kHz, 4L-FSK uses the direct detector plus a bounded 16-tap windowed-sinc fractional timing path; both validate packet CRC and suppress duplicate timing hypotheses.

`tools/kgstv` can build without the full application's Hamlib/audio dependencies:

```sh
cmake -S tools/kgstv -B build-kgstv -DCMAKE_BUILD_TYPE=Release
cmake --build build-kgstv -j2
ctest --test-dir build-kgstv --output-on-failure
```

The standalone core requires Qt6 Core/Gui (6.2+); the application retains its existing Qt6 6.5+ requirement. CTest covers fixed vectors, malformed inputs, framing/chunking/reset, both modulations, image/text exchange, cancellation, simulated loss/BSR/repair and complete WAV workflows. `tests/kgstv/run_ui.py BUILD` reuses a completed Unix Makefiles CMake build's application objects for device-free real-widget checks.

Two optional user-supplied recordings are not distributed: `KG-STV_4LFSK_Norm.mp3` (SHA-256 `6f65c15ba9065df2e0c781bbe9b3c19d890079cdc77985b96140e58b445613e7`) and `KG-STV_4L-FSK_text.mp3` (`aa79e18f41f1172d88648f863e759c8fba530772f921a562fb314ace57f492f7`). They establish external reception evidence, not a complete original interoperability guarantee.

## Limits

This mode is experimental. Original-application reception of our transmissions, original BSR exchanges, non-ASCII encoding, exact rendering/rounding, noisy channels and multiple/interleaved senders still need independent tests. Real audio/CAT/PTT and platform-specific Windows/macOS behavior need hardware validation. No reference Windows executable is required or executed by the native implementation.


## Port validation (local Linux, Qt 6.10.2)

- Full CMake application build passed using a separately built current Hamlib. The local package set lacks libavif, so this build does not validate AVIF over DRM; that pre-existing optional feature was not changed.
- Native core/controller and WAV image/text/loss/BSR tests passed. Real RX/TX widget tests passed with no sound/CAT/XML-RPC initialization; the screenshot was inspected.
- Both supplied MP3s passed through the **real application media reader and receive thread**. The image recovers 300/300 blocks; its PNG pixel stream matches the independent native decode (DPI metadata differs between QCoreApplication and QApplication). The text is received exactly once as `TEST3>The Quick Brown Fox Jumps Over The Lazy Dog 0123456789`.
- Real application image TX to 48 kHz stereo WAV and subsequent file RX recovered 300/300 blocks. Tests use isolated HOME/XDG settings and no devices. These loopbacks are not original-application or on-air compatibility tests.

`python3 tests/kgstv/test_application.py APP OFFLINE [REFERENCE_DIRECTORY]` checks both modulations through the full file TX/RX path, compares against the offline codec, exercises text/error/no-signal cases and verifies unchanged settings. The optional reference directory is hash-checked and is never downloaded. Linux full builds also register the synthetic application check in CTest. The standalone CI job checks the native backend without requiring Hamlib or a sound device.
