#!/usr/bin/env python3
"""Verify the Windows package bundles every non-system DLL it actually needs.

Checking only qsstv.exe's own imports (or those plus the Qt plugins' own imports) is not
enough: a bundled DLL like Qt6Gui.dll or Qt6Network.dll can itself depend on further
non-system DLLs (ICU, fontconfig/freetype/harfbuzz, OpenSSL, pcre2, zlib, ...) that never
show up until something using them actually runs. That's exactly how "icui18n77.dll was
not found" made it into a published release: the DLL list in src/CMakeLists.txt was
checked against qsstv.exe's and each plugin's own direct imports, not the full recursive
closure. This script walks the whole closure with objdump instead of eyeballing it.

Usage:
    tools/check-win-deps.py build-mingw64/qsstv.exe [build-mingw64/qsstv-9.0-win64/...]

With just the exe, it reports the full set of non-system DLLs needed (including Qt
plugins found under the mingw sysroot) so you can compare it against
QSSTV_WIN_RUNTIME_DLLS in src/CMakeLists.txt. Pass the unzipped package directory too
(or use --package-dir) and it instead reports PASS/FAIL: whether everything the closure
needs is actually present there.
"""
import argparse
import os
import subprocess
import sys

SYSROOT = "/usr/x86_64-w64-mingw32/sys-root/mingw"
OBJDUMP = "x86_64-w64-mingw32-objdump"

# Plugins this app loads at runtime (not visible in qsstv.exe's own import table --
# Qt loads them dynamically) -- keep in sync with the install(FILES ... DESTINATION
# platforms/multimedia/imageformats/styles) rules in src/CMakeLists.txt.
PLUGIN_DLLS = [
    f"{SYSROOT}/lib/qt6/plugins/platforms/qwindows.dll",
    f"{SYSROOT}/lib/qt6/plugins/multimedia/windowsmediaplugin.dll",
    f"{SYSROOT}/lib/qt6/plugins/imageformats/qjpeg.dll",
    f"{SYSROOT}/lib/qt6/plugins/imageformats/qwebp.dll",
    f"{SYSROOT}/lib/qt6/plugins/styles/qmodernwindowsstyle.dll",
]


def dll_names(path):
    out = subprocess.run([OBJDUMP, "-p", path], capture_output=True, text=True, check=True).stdout
    return [line.split("DLL Name:")[1].strip() for line in out.splitlines() if "DLL Name:" in line]


def find_in_sysroot(name):
    search_dirs = [f"{SYSROOT}/bin"]
    for root, _dirs, _files in os.walk(f"{SYSROOT}/lib/qt6/plugins"):
        search_dirs.append(root)
    for d in search_dirs:
        if not os.path.isdir(d):
            continue
        for f in os.listdir(d):
            if f.lower() == name.lower():
                return os.path.join(d, f)
    return None


def transitive_closure(seed_paths):
    """Returns {dll_name: resolved_sysroot_path} for every non-system DLL reachable
    from the seeds, found by walking objdump -p to a fixed point."""
    resolved = {}
    queue = list(seed_paths)
    visited_files = set()
    while queue:
        p = queue.pop(0)
        if p in visited_files:
            continue
        visited_files.add(p)
        for name in dll_names(p):
            if name in resolved:
                continue
            path = find_in_sysroot(name)
            resolved[name] = path
            if path:
                queue.append(path)
    return {name: path for name, path in resolved.items() if path}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe", help="path to the cross-compiled qsstv.exe")
    ap.add_argument("--package-dir", help="unzipped package directory to check for completeness")
    args = ap.parse_args()

    closure = transitive_closure([args.exe] + PLUGIN_DLLS)

    if not args.package_dir:
        print(f"Full non-system DLL closure ({len(closure)} entries):")
        for name in sorted(closure):
            print(f"  {name}")
        print("\nCompare against QSSTV_WIN_RUNTIME_DLLS in src/CMakeLists.txt.")
        return 0

    missing = []
    for name in closure:
        # The 4 plugin DLLs themselves live under platforms/multimedia/imageformats/styles/
        # in the package, not the top level -- everything else should be at the top level.
        candidates = [os.path.join(args.package_dir, name)]
        for sub in ("platforms", "multimedia", "imageformats", "styles"):
            candidates.append(os.path.join(args.package_dir, sub, name))
        if not any(os.path.exists(c) for c in candidates):
            missing.append(name)

    if missing:
        print(f"FAIL: {len(missing)} required DLL(s) missing from {args.package_dir}:")
        for name in sorted(missing):
            print(f"  {name}")
        return 1
    print(f"PASS: all {len(closure)} required DLLs are present in {args.package_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
