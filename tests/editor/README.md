# Template editor tests

Tests of the template editor in `src/editor/` and of the picture hand-off in `src/utils/externaleditor.*`.

| Script | What it checks | Needs |
|---|---|---|
| `run.sh` | The editor built on its own (no hamlib, no application globals) and driven offscreen with real mouse and keyboard events: undo/redo of every command, random operation sequences, merging and the modified flag, snapping, resizing/rotating, the clipboard, the properties and layers panels, file handling, text being typed when something else happens, ~2800 random input steps, the external-editor hand-off, and one check per defect of the old editor | `g++`, Qt6 Core/Gui/Widgets/Svg/Test headers, Qt's `moc`/`rcc` |
| `app.sh` | The editor inside the **real application objects** (main window, Transmit tab, gallery, dispatcher), offscreen with a throwaway `HOME`: opens it from the TX tab's *Edit...* button, saves, checks that the gallery and the TX template list follow and keep their selection, closes and reopens, and hands a picture to a fake external editor | an existing `build-cmake` tree (it links the application's object files) |

`EDITOR_MONKEY_SESSIONS=200 tests/editor/run.sh` runs a longer random-input session (default 8).

`build.sh <out-dir> <main.cpp>...` builds the editor sources into one binary per main file; `shot.cpp` is not a test: it renders the editor in several states to PNG
(used for the User's Guide screenshots and for looking at dark mode): `tests/editor/build.sh /tmp/out tests/editor/shot.cpp && /tmp/out/shot /tmp/shots tests/template/fixtures [dark]`.

## Defects of the old editor and where they are covered

| Old editor | Test (`run.sh` unless noted) |
|---|---|
| Edit > Paste with nothing copied crashed; Copy twice crashed | `clipboard`: paste with an empty/damaged clipboard, copy/cut with no selection |
| Opening a template and closing it asked to save | `files`: `maybeSave()` right after `openFile()` asks nothing |
| Cancel in the save prompt closed the window and locked the editor out | `files`: `close()` with *Cancel* stays open; `app.sh`: the editor opens again after closing |
| *Save file* wrote PNG data under a `.jpg` name; New kept the image type; *Save as Image* baked in the guide frame | the editor only writes `.qtempl` (`files`: save, New) |
| File > Open merged into the current scene | `files`: opening replaces the document |
| Moving a shear slider zeroed the rotation | `properties panel` |
| Clicking a text with an uninstalled font rewrote the font | `properties panel`: font and undo stack untouched by selecting |
| Inserted pictures were 100 x 100 | `images`: aspect kept, half the canvas |
| Ctrl+A was *Clear All* | `the window`: Select All selects, nothing is deleted |
| A byte-flipped `.templ` crashed inside Qt | `tests/template/run.sh` (fuzz of the importer) and `files`: a damaged file is refused |
