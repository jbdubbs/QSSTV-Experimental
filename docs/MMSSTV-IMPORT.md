# MMSSTV template import

**Experimental: not yet live-tested in an operator’s normal workflow or on-air use.** Automated tests and external fixture conversion are the available evidence; visual comparison with Windows MMSSTV remains open.

The Qt6 template editor imports MMSSTV `.mtm` files as native `.qtempl` documents. This ports the independently developed QSSTV 9.6 importer to QSSTV-Experimental's document model, renderer and editor. It does not change the application's version or configuration paths.

## Single import

Choose **File → Import MMSSTV template…** in the template editor. Review the rendered preview and conversion warnings, then accept. Replacing an unsaved document uses the editor's usual Save/Discard/Cancel prompt. The imported document has no filename and is marked unsaved. Save writes a new `.qtempl`; the original `.mtm` stays untouched. The saved file is available in the template gallery and TX selection.

The main canvas shows editable objects against the imported background. The import preview, Size preview, gallery and transmitted overlay use the complete rendering, including sampled transparency. Inspect Size preview with your actual transmission size and values before sending.

## Batch import

Choose **File → Import MMSSTV templates (batch)…**, select multiple files and choose the destination folder (normally the configured templates folder). Confirm conversion. Each successful conversion is published as a new `.qtempl`, with `_2`, `_3`, etc. when names are occupied. Existing files and symbolic links are preserved. A report lists successful files, warnings and failures; one failed file does not stop the others. The current editor document and its unsaved changes are preserved. Gallery/TX refresh uses the same `templateSaved` signal as ordinary saving.

## Supported content and limits

The bounded reader follows the on-disk fields of `CDrawGroup`, `CDrawText`, `CDrawPic` and color bars in the [original MMSSTV source](https://github.com/n5ac/mmsstv/blob/master/Draw.cpp). Supported group versions are 0–2, text 0–7, pictures 1–5 and color bars 1–3, including the MMSSTV 1.13A reference templates tested locally.

- Lines, rectangles, filled rectangles, ellipses and text remain editable; groups are flattened in paint order. Simple text gradients/contours remain native properties.
- Embedded BMPs, tiled text textures, picture color keys and rounded/ellipse/polygon image masks are retained. Image masks and rounded frames are baked into pixels; mask vertices and rounded-frame geometry are not separately editable. Solid and gradient fill controls replace a text texture with the selected fill.
- Root background and sampled transparency are retained: the key color is sampled after composing the overlay, and removed from the whole overlay before it is placed over the TX picture. Removing it separately from each object would reveal lower objects incorrectly. Resizing the canvas moves the sample point proportionally.
- History-image placeholders use QSSTV's latest received image. Their dynamic masks are not retained. Color-bar gradients become images. Bevels, complex text effects, rotation placement, picture cropping and Windows system colors are simplified with warnings. Font substitution and rasterization can change appearance.
- Windows-1252 and Windows-1251 decoding is built in. Shift-JIS, GBK and Big5 require the corresponding named decoder in the Qt build; unavailable decoders fail explicitly. Other declared character sets use Windows-1252 with a warning.
- Unknown/OLE/custom objects and sound bars fail explicitly. Nothing embedded is executed. File length (32 MiB), bitmap dimensions/aggregate pixels, object counts, recursion, strings and polygon vertices are bounded. Conversion rejects objects exceeding native text/font/geometry limits rather than silently truncating them. Native output is limited to 1,000 objects and 64 MiB.

Original Windows MMSSTV rendering has not been compared pixel for pixel. Successful import and native round-tripping establish persistence and internal consistency, not complete visual equivalence or Windows/macOS validation.

## Macros

| MMSSTV | Native | Meaning |
|---|---|---|
| `%m`, `%c`, `%C`, `%r`, `%v` | unchanged | Own/contact/lowercase-contact callsign, report, version |
| `%n` | `%o` | Contact operator |
| `%T`, `%t` | `%t`, `%T` | UTC time with/without colon |
| `%D`, `%L` | `%d`, `%D` | UTC/local date (`yyyy/MM/dd`, warning shown) |
| `%U`, `%u` | unchanged | Local time with/without colon |
| `%R`, `%N` | unchanged | First three report characters and exact remainder |

Time fields capture one instant; local fields use the operating system's time zone. For `595#007`, `%R` is `595` and `%N` is `#007`. Empty/short reports do not invent MMSSTV's historical default `595`; the preview warns about that difference. Unsupported fields (including received-report `%s`/`%M` and contact-QTH `%q`) are escaped as visible literals and reported. They never expand to unrelated native macros.

## Native compatibility

Ordinary existing `.qtempl` files still use version 1. Sampled transparency and textured text require **format version 2**. Earlier QSSTV-Experimental builds reject version 2 instead of silently losing these properties. This format marker is separate from the application version. The QSSTV 9.6 binary `.templ` format with its optional `MMCK` record is not the output format here; import the original `.mtm` directly to retain these features. Legacy ordinary `.templ` loading remains available.

## Verification

See [tests/mmsstv/README.md](../tests/mmsstv/README.md). Linux Qt6 validation on 2026-10-28 covers the full application build, the real editor objects, preview acceptance/cancellation, unsaved-document protection, failed-save state, native save/reload/render, batch name collisions (including dangling symlinks), mixed failures, source preservation, transparency through overlapping objects, macros, shapes, textures, masks, every truncated prefix and all five original MMSSTV `def1.mtm`–`def5.mtm` files kept outside the repository.

The pre-existing template regression suite reports 179 passes and seven legacy golden-image differences in this local Qt 6.10.2 environment. An unmodified `origin/main` checkout produces the same seven differences and PSNR values; no golden images were changed to hide them. Independent original-render and Windows/macOS checks remain open.
