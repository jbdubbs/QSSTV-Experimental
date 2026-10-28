"""Link the device-free widget test against an existing Unix CMake application build.
Usage: python3 tests/kgstv/run_ui.py BUILD
The production objects are reused, not rebuilt with test substitutes.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys

build = Path(sys.argv[1]).resolve()
flags = {}
for line in (build / 'CMakeFiles/qsstv.dir/flags.make').read_text().splitlines():
    if line.startswith(('CXX_DEFINES =', 'CXX_INCLUDES =', 'CXX_FLAGS =')):
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
source = Path(__file__).with_name('test_ui.cpp').resolve()
obj = build / 'kgstv_widget_test.o'
subprocess.run(['c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'],
                '-c', str(source), '-o', str(obj)], cwd=build, check=True)
command = shlex.split((build / 'CMakeFiles/qsstv.dir/link.txt').read_text())
command[command.index('-o') + 1] = str(build / 'kgstv_widget_test')
found = False
for i, word in enumerate(command):
    if word.endswith('/main.cpp.o'):
        command[i] = str(obj)
        found = True
if not found:
    raise SystemExit('Production main object not found')
subprocess.run(command, cwd=build, check=True)
subprocess.run([str(build / 'kgstv_widget_test')], cwd=build,
               env=dict(os.environ, QT_QPA_PLATFORM='offscreen'), check=True)
