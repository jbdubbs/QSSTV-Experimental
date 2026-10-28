"""Device-free KG-STV tests through the real Qt6 application's file RX/TX paths.
Usage: python3 tests/kgstv/test_application.py APP OFFLINE [REFERENCE_DIRECTORY]
The optional directory contains the two user-supplied MP3s; never downloaded/bundled.
"""
from pathlib import Path
import hashlib
import os
import struct
import subprocess
import sys
import tempfile
import wave
import zlib

app, offline = (str(Path(p).resolve()) for p in sys.argv[1:3])
references = Path(sys.argv[3]) if len(sys.argv)>3 else None


def png_pixels(path):
    data=Path(path).read_bytes(); at=8; packed=b''; dimensions=None
    assert data[:8]==b'\x89PNG\r\n\x1a\n'
    while at<len(data):
        size=struct.unpack_from('>I',data,at)[0];kind=data[at+4:at+8];content=data[at+8:at+8+size]
        if kind==b'IHDR':dimensions=content
        if kind==b'IDAT':packed+=content
        at+=12+size
    # The same Qt/libpng encoder is used in this test. Ignore DPI/other metadata.
    return dimensions,zlib.decompress(packed)


with tempfile.TemporaryDirectory(prefix='qsstv-experimental-kgstv-') as temp:
    root=Path(temp);home=root/'home';home.mkdir()
    settings=home/'.config/qsstv-experimental/qsstv-experimental.conf';settings.parent.mkdir(parents=True)
    original=b'[KGSTV]\nfourLevel=false\nquality=8\nfec=true\n\n[SOUND]\nduplicateChannel=false\nrxclock=48048\ntxclock=47952\n\n[SENTINEL]\nunknown=keep\n'
    settings.write_bytes(original)
    env=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(home/'.config'),XDG_DATA_HOME=str(home/'.local/share'),QT_QPA_PLATFORM='offscreen')

    def run(binary,*args,expected=0):
        result=subprocess.run([binary,*map(str,args)],env=env,capture_output=True,text=True,timeout=240)
        assert result.returncode==expected,(args,result.returncode,result.stdout,result.stderr)
        assert settings.read_bytes()==original,'Headless KG-STV changed persistent settings'
        return result

    for args in [('--batch','--kgstv','--drm','missing.wav'),('--batch','--kgstv-four-level','missing.wav')]:
        run(app,*args,expected=2)
    # Independent 320x240 BMP fixture, no production codec used to create it.
    image=root/'bars.bmp';pixels=bytearray()
    for y in range(240):
        for x in range(320):
            pixels.extend(((x//40)*32,(y//40)*40,255-(x//40)*32))
    image.write_bytes(b'BM'+struct.pack('<IHHI',54+len(pixels),0,0,54)+struct.pack('<IiiHHIIiiII',40,320,240,1,24,0,len(pixels),2835,2835,0,0)+pixels)
    for four in (False,True):
        flags=['--kgstv-four-level'] if four else []
        native_flags=['--four-level'] if four else []
        recorded=root/f'production-{four}.wav'
        encode_args=['--encode='+str(image)] if four else ['--encode',image]
        run(app,*encode_args,'--kgstv',*flags,'--wav-out',recorded)
        with wave.open(str(recorded)) as w:
            assert w.getnchannels()==2 and w.getframerate()==48000 and w.getnframes()>0
        output=root/f'roundtrip-{four}'
        result=run(app,'--batch','--kgstv',*flags,'--out-dir',output,recorded)
        assert '300/300 blocks' in result.stdout
        assert len(list(output.glob('*.png')))==1
        # Compare to independent use of the shared codec at the same quantization.
        baseline_wav=root/f'baseline-{four}.wav';baseline_png=root/f'baseline-{four}.png'
        run(offline,'encode',image,baseline_wav,*native_flags)
        run(offline,'decode',baseline_wav,baseline_png,*native_flags)
        assert png_pixels(next(output.glob('*.png')))==png_pixels(baseline_png)
    text=root/'message.txt';text.write_text('<plain> native text 0123456789',encoding='ascii')
    signal=root/'text.wav';run(offline,'text',text,signal,'--four-level')
    out=root/'text-output';run(app,'--batch','--kgstv','--kgstv-four-level','--out-dir',out,signal)
    assert [p.read_text() for p in out.glob('*.txt')]==[text.read_text()+'\n']
    empty=root/'empty.wav'
    with wave.open(str(empty),'wb') as w:w.setparams((1,2,12000,0,'NONE','not compressed'));w.writeframes(b'\0\0'*12000)
    run(app,'--batch','--kgstv','--out-dir',root/'empty-output',empty,expected=1)
    if references:
        for name,sha,kind in [('KG-STV_4LFSK_Norm.mp3','6f65c15ba9065df2e0c781bbe9b3c19d890079cdc77985b96140e58b445613e7','image'),
                              ('KG-STV_4L-FSK_text.mp3','aa79e18f41f1172d88648f863e759c8fba530772f921a562fb314ace57f492f7','text')]:
            source=references/name;assert hashlib.sha256(source.read_bytes()).hexdigest()==sha
            output=root/f'external-{kind}'
            result=run(app,'--batch','--kgstv','--kgstv-four-level','--out-dir',output,source)
            if kind=='image':assert '300/300 blocks' in result.stdout and len(list(output.glob('*.png')))==1
            else:assert [p.read_text() for p in output.glob('*.txt')]==['TEST3>The Quick Brown Fox Jumps Over The Lazy Dog 0123456789\n']
print('PASS: real Qt6 file TX/RX, stereo 48 kHz WAV, both modulations, 300/300 images, independent codec comparison, standalone plain text, invalid options, no-signal exit and unchanged settings; no card/CAT/PTT.')
