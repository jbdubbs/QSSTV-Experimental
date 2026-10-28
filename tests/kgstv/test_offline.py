"""Exercise experimental KG-STV WAV encode/decode/text/BSR, without radio devices.
Usage: python3 tests/kgstv/test_offline.py /path/to/kgstv-offline
All signal fixtures are synthetic; this does not establish original interoperability.
"""
import hashlib
import json
import os
import shutil
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import wave

binary = str(Path(sys.argv[1]).resolve())


def run(*arguments, expected=0):
    result = subprocess.run([binary, *map(str, arguments)], capture_output=True, text=True, timeout=180)
    if result.returncode != expected:
        raise AssertionError(f'{arguments}: exit {result.returncode}\n{result.stdout}\n{result.stderr}')
    return result


with tempfile.TemporaryDirectory(prefix='qsstv-kgstv-offline-') as temporary:
    root = Path(temporary)
    # Independent uncompressed BMP fixture, generated without production image code.
    image = root / 'source image.bmp'
    pixels = bytearray()
    for y in reversed(range(240)):
        for x in range(320):
            pixels.extend((128, y * 255 // 239, x * 255 // 319))
    bmp_header = b'BM' + struct.pack('<IHHI', 54 + len(pixels), 0, 0, 54)
    bmp_header += struct.pack('<IiiHHIIiiII', 40, 320, 240, 1, 24, 0, len(pixels), 0, 0, 0, 0)
    image.write_bytes(bmp_header + pixels)
    original_hash = hashlib.sha256(image.read_bytes()).hexdigest()
    signal, baseline = root / 'image.wav', root / 'baseline.png'
    run('encode', image, signal, '--callsign', 'OE6PLD')
    complete = run('decode', signal, baseline)
    assert 'Received 300/300' in complete.stdout
    with wave.open(str(signal), 'rb') as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 12000)
        frames = bytearray(wav.readframes(wav.getnframes()))
    # Erase a 1-second segment inside the image without altering sample timing.
    start = min(len(frames) // 2, 12000 * 2 * 3)
    start -= start % 2
    frames[start:start + 24000] = bytes(min(24000, len(frames) - start))
    damaged = root / 'damaged.wav'
    with wave.open(str(damaged), 'wb') as wav:
        wav.setparams((1, 2, 12000, 0, 'NONE', 'not compressed'))
        wav.writeframes(frames)
    requests, partial = root / 'requests.wav', root / 'partial.png'
    incomplete = run('decode', damaged, partial, '--bsr-out', requests, expected=3)
    assert 'Received 300/300' not in incomplete.stdout
    assert requests.is_file()
    repairs, recovered = root / 'repairs.wav', root / 'recovered.png'
    run('repair', image, requests, repairs)
    complete = run('decode', damaged, recovered, '--append', repairs)
    assert 'Received 300/300' in complete.stdout
    assert recovered.read_bytes() == baseline.read_bytes()
    assert hashlib.sha256(image.read_bytes()).hexdigest() == original_hash
    # Existing outputs are refused, including during signal creation.
    signal_hash = hashlib.sha256(signal.read_bytes()).hexdigest()
    run('encode', image, signal, expected=1)
    assert hashlib.sha256(signal.read_bytes()).hexdigest() == signal_hash
    baseline_bytes = baseline.read_bytes()
    run('decode', signal, baseline, expected=1)
    assert baseline.read_bytes() == baseline_bytes
    # Standalone ASCII text without a selected image, with optional identification.
    for four_level in (False, True):
        extra = ['--four-level'] if four_level else []
        message = root / f'message-{four_level}.txt'
        message.write_text('CQ CQ de OE6PLD\nStandalone KG-STV text test.\n', encoding='ascii')
        text_wave = root / f'text-{four_level}.wav'
        text_output = root / f'received-{four_level}.txt'
        run('text', message, text_wave, '--callsign', 'OE6PLD', *extra)
        run('decode-text', text_wave, text_output, *extra)
        assert message.read_text() in text_output.read_text()
        assert 'Identification: OE6PLD' in text_output.read_text()
    invalid = root / 'non-ascii.txt'
    invalid.write_text('Grüße', encoding='utf-8')
    unwanted = root / 'must-not-exist.wav'
    run('text', invalid, unwanted, expected=1)
    assert not unwanted.exists()
    invalid.write_bytes(b'A' * 201)
    run('text', invalid, unwanted, expected=1)
    assert not unwanted.exists()
    invalid.write_bytes(b'')
    run('text', invalid, unwanted, expected=1)
    assert not unwanted.exists()
    invalid.write_bytes(b'A' * 200)
    boundary_wave, boundary_text = root / 'boundary.wav', root / 'boundary.txt'
    run('text', invalid, boundary_wave, '--no-fec', '--four-level')
    run('decode-text', boundary_wave, boundary_text, '--four-level')
    assert 'A' * 200 in boundary_text.read_text()
    # Appending a duplicate pass must preserve the first accepted image.
    duplicate_output = root / 'duplicate.png'
    run('decode', signal, duplicate_output, '--append', signal)
    assert duplicate_output.read_bytes() == baseline_bytes
    # Malformed RIFF length, unsupported sample format and a truncated sample stream.
    for index, data in enumerate((b'RIFF', signal.read_bytes()[:-1], b'not a wave' * 10)):
        malformed = root / f'malformed-{index}.wav'
        malformed.write_bytes(data)
        output = root / f'malformed-{index}.png'
        run('decode', malformed, output, expected=1)
        assert not output.exists()
    assert not list(root.glob('.kgstv-offline-*'))
    if os.environ.get('QSSTV_TEST_ARTIFACTS'):
        artifacts = Path(os.environ['QSSTV_TEST_ARTIFACTS']) / 'kgstv-offline-fixtures'
        artifacts.mkdir(parents=True, exist_ok=True)
        selected = [image, signal, baseline, damaged, requests, partial, repairs, recovered,
                    root / 'message-False.txt', root / 'text-False.wav', root / 'text-True.wav']
        hashes = {}
        for source in selected:
            shutil.copy2(source, artifacts / source.name)
            hashes[source.name] = hashlib.sha256(source.read_bytes()).hexdigest()
        (artifacts / 'manifest.json').write_text(json.dumps({
            'origin': 'Generated by native QSSTV experimental KG-STV offline tests',
            'interoperability': 'Synthetic signals only; original KG-STV compatibility is not established',
            'partial_result': incomplete.stdout.strip(),
            'repair_result': complete.stdout.strip(),
            'sha256': hashes,
        }, indent=2) + '\n')

print('PASS: synthetic WAV image RX/TX, erased audio/BSR requests/repairs/identical recovery, standalone MSK/4L-FSK text, source/output preservation and malformed input. Original interoperability remains unverified.')
