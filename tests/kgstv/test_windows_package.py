"""Run the application extracted from the Fedora-built NSIS installer on Windows.
No installation, audio streams or CAT/PTT: only --help/--version and KG-STV files.
The offline helper must be copied next to the EXE solely for this CI check.
"""
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile
import wave

assert sys.platform == 'win32', 'This verification requires a real Windows runner'
app = Path(sys.argv[1]).resolve()
offline = app.with_name('kgstv-offline.exe')
assert app.is_file() and offline.is_file()
# Keep Windows itself, remove every compiler/Qt/MSYS2 directory from the search path.
env = dict(os.environ, PATH=os.path.join(os.environ['SystemRoot'], 'System32') + os.pathsep + os.environ['SystemRoot'],
           QT_QPA_PLATFORM='windows')
for key in ('QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QML2_IMPORT_PATH', 'QML_IMPORT_PATH'):
    env.pop(key, None)

def run(binary, *args, expected=0):
    result = subprocess.run([str(binary), *map(str, args)], env=env, capture_output=True,
                            text=True, timeout=240, creationflags=subprocess.CREATE_NO_WINDOW)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    return result

run(app, '--version')
run(app, '--help')
with tempfile.TemporaryDirectory(prefix='QSSTV KGSTV Unicode ä ') as temp:
    root = Path(temp)
    home = root / 'home'; home.mkdir(); env['HOME'] = str(home)
    image = root / 'source image.bmp'
    pixels = bytearray()
    for y in range(240):
        for x in range(320):
            pixels.extend(((x // 40) * 32, (y // 40) * 40, 255 - (x // 40) * 32))
    image.write_bytes(b'BM' + struct.pack('<IHHI', 54 + len(pixels), 0, 0, 54)
        + struct.pack('<IiiHHIIiiII', 40, 320, 240, 1, 24, 0, len(pixels), 2835, 2835, 0, 0) + pixels)
    for four in (False, True):
        flags = ['--kgstv-four-level'] if four else []
        native = ['--four-level'] if four else []
        signal = root / f'production-{four}.wav'
        run(app, '--encode', image, '--kgstv', *flags, '--wav-out', signal)
        with wave.open(str(signal)) as wav:
            assert wav.getnchannels() == 2 and wav.getframerate() == 48000 and wav.getnframes() > 0
        output = root / f'decoded-{four}'
        run(app, '--batch', '--kgstv', *flags, '--out-dir', output, signal)
        assert len(list(output.glob('*.png'))) == 1
        reference_signal = root / f'reference-{four}.wav'
        reference_image = root / f'reference-{four}.png'
        run(offline, 'encode', image, reference_signal, *native)
        run(offline, 'decode', reference_signal, reference_image, *native)
        # Qt uses the same PNG encoder for both paths; ignore DPI/ancillary chunks.
        def png_content(path):
            import zlib
            data = path.read_bytes(); at = 8; packed = b''; dimensions = None
            assert data[:8] == b'\x89PNG\r\n\x1a\n'
            while at < len(data):
                size = struct.unpack_from('>I', data, at)[0]
                kind, chunk = data[at+4:at+8], data[at+8:at+8+size]
                if kind == b'IHDR': dimensions = chunk
                if kind == b'IDAT': packed += chunk
                at += size + 12
            return dimensions, zlib.decompress(packed)
        assert png_content(next(output.glob('*.png'))) == png_content(reference_image)
    message = root / 'message.txt'; message.write_text('CQ native Windows KG-STV 0123456789', encoding='ascii')
    signal = root / 'text.wav'; run(offline, 'text', message, signal, '--four-level')
    output = root / 'text-output'
    run(app, '--batch', '--kgstv', '--kgstv-four-level', '--out-dir', output, signal)
    assert [p.read_text() for p in output.glob('*.txt')] == [message.read_text() + '\n']
print('PASS: NSIS payload, clean Windows PATH, packaged Qt/plugins/DLLs, both modulations, real file TX/RX, image pixels and text. No audio/CAT/PTT.')
