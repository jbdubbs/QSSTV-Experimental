# MMSSTV importer tests

Build the production Qt6 template model, renderer and editor without sound cards, Hamlib or application initialization:

```sh
cmake -S tests/mmsstv -B build-mmsstv -DCMAKE_BUILD_TYPE=Release
cmake --build build-mmsstv -j2
ctest --test-dir build-mmsstv --output-on-failure
```

Requires Qt6 Core/Gui/Widgets/Svg development packages (Qt 6.4+ for these standalone tests). The application still requires Qt 6.5+. Tests run offscreen with an isolated HOME/config directory and never initialize CAT or audio. CI runs this same target on Ubuntu 24.04.

Optional original external fixtures (not downloaded or redistributed by the test):

```sh
QT_QPA_PLATFORM=offscreen build-mmsstv/mmsstv_import_test /path/to/mmsstv-reference
```

The directory must contain `def1.mtm` through `def5.mtm` from MMSSTV. Their native save/reload renders must match the initial conversion exactly. This is not a pixel comparison against Windows MMSSTV itself.

To link the same test against an already built production application (Unix Makefiles):

```sh
python3 tests/mmsstv/run_application.py /path/to/application-build [/path/to/mmsstv-reference]
```

Coverage includes bounded parsing and every truncated fixture prefix, unsupported records, editable shapes/text, gradients and contours, embedded images and tiled text, ellipse/polygon masks, sampled transparency through overlapping objects, version 1/2 output, macro semantics, native save/reopen and TX rendering. Actual editor dialogs cover preview cancellation/acceptance, refusal to discard unsaved work, failed-save modified state and batch multi-selection/destination/confirmation cancellation, mixed success and error reporting. Batch conversions preserve the current editor document, source bytes and occupied output names (including dangling symlinks).

Run `tests/template/run.sh` and `tests/editor/run.sh` for the host's broader template/editor suites. Local Qt 6.10.2 verification of the former has seven legacy golden-image differences (179 passes), reproduced without these changes on `origin/main`. Do not replace reference images merely to make that environment pass.
