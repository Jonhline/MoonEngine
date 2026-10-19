"""Decode generated compressed audio through the player's own decoder.

FFmpeg (the program) only encodes the fixtures; the player decodes them with the libav
linked into the binary. Each must come out as the 440 Hz tone at 48 kHz; a 400 Hz
MP3 must be rejected, which proves the frequency check discriminates."""
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

exe, output = (Path(p).resolve() for p in sys.argv[1:3])
output.mkdir(parents=True, exist_ok=True)
candidate = Path(os.environ.get("APPDATA", "")) / "MoonEngine/MoonEngine/ffmpeg.exe"
ffmpeg = str(candidate) if candidate.is_file() else shutil.which("ffmpeg")
if not ffmpeg:
    raise SystemExit("No existing FFmpeg decoder: compressed-format validation cannot run")
with tempfile.TemporaryDirectory(prefix="moon-music-codecs-") as folder:
    root = Path(folder)
    source = root / "generated.wav"
    def tone(path, hz):
        with wave.open(str(path), "wb") as f:
            f.setparams((2, 2, 44100, 0, "NONE", "not compressed"))
            f.writeframes(b"".join(struct.pack("<hh", int(math.sin(i * 2 * math.pi * hz / 44100) * 8000),
                                              int(math.sin(i * 2 * math.pi * hz / 44100) * 8000))
                                   for i in range(44100)))
    tone(source, 440)
    for extension in ("mp3", "flac", "ogg", "m4a"):
        media = root / ("generated." + extension)
        encode = subprocess.run([ffmpeg, "-nostdin", "-v", "error", "-i", str(source), str(media)],
                                capture_output=True, timeout=20)
        assert encode.returncode == 0, encode.stderr.decode("utf-8", "replace")
        result = subprocess.run([str(exe), str(output / extension), "--compressed", str(media)],
                                capture_output=True, timeout=30)
        (output / (extension + ".log")).write_bytes(result.stdout + result.stderr)
        assert result.returncode == 0, (extension, result.stderr.decode("utf-8", "replace"))
        print("PASS streamed", extension, "to stereo 48 kHz PCM")
    result = subprocess.run([str(exe), str(output / "wav44"), "--compressed", str(source)],
                            capture_output=True, timeout=30)
    (output / "wav44.log").write_bytes(result.stdout + result.stderr)
    assert result.returncode == 0, ("wav 44.1 kHz", result.stderr.decode("utf-8", "replace"))
    print("PASS resampled 44.1 kHz WAV to stereo 48 kHz PCM")
    wrong = root / "wrong.wav"
    tone(wrong, 400)
    media = root / "wrong.mp3"
    encode = subprocess.run([ffmpeg, "-nostdin", "-v", "error", "-i", str(wrong), str(media)],
                            capture_output=True, timeout=20)
    assert encode.returncode == 0, encode.stderr.decode("utf-8", "replace")
    result = subprocess.run([str(exe), str(output / "wrong"), "--compressed", str(media)],
                            capture_output=True, timeout=30)
    (output / "wrong.log").write_bytes(result.stdout + result.stderr)
    assert result.returncode != 0, "a 400 Hz tone passed the 440 Hz check"
    print("PASS rejected a 400 Hz tone")
