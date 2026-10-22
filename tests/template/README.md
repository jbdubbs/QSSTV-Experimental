# Template tests

Tests of the template engine in `src/template/`: the document model, the JSON (`.qtempl`) format, the importer for the
legacy binary `.templ` format, and the headless renderer that the transmit path and the gallery use.

| Script | What it checks | Needs |
|---|---|---|
| `run.sh` | Renderer parity with the legacy editor (golden images), JSON round trip, validation and limits, macro expansion, fuzzing of the legacy loader | `g++`, Qt6 headers |
| `cli.sh` | `--convert-template` / `--render-template` of the real application, error handling and exit codes | the built `qsstv-experimental`, `g++` |

`TPL_FUZZ_ROUNDS=100000 tests/template/run.sh` runs a longer fuzz (default 6000 mutations of the synthetic template plus every truncation).

## Fixtures and golden images

`fixtures/synthetic.templ` is a legacy-format template with every item type (gradient bars, rotated and sheared shapes and text, macros,
multi-line text, an image with a border, the replay slot, z-order). `golden/` holds what the **legacy editor** (`src/editor`, before the rebuild)
rendered from it: the frame it is laid over (`base_WxH.png`), the result with and without a replay picture at three sizes, and the gallery thumbnail.
`fixtures/Montserrat-Bold.otf` (SIL OFL, see `Montserrat-OFL.txt`) is loaded by the tests so the text renders the same on every machine.

The new renderer reproduces those images bit for bit (PSNR 999 dB). The same was checked against four real user templates (not committed).

`legacy-golden/gen.cpp` regenerates the fixtures and goldens from the legacy classes. It links against the object files of a build made **before**
the legacy editor was removed (see `build.sh`), so it only works on such a checkout; it is kept as the record of where the goldens came from.
