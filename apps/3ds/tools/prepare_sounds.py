#!/usr/bin/env python3
"""Optional development-only MP3 conversion utility (not needed by users).

Usage: python tools/prepare_sounds.py "C:/Users/Seth/Downloads"
Production builds embed all 11 converted samples; no SD sound pack is needed.
This tool does not download or redistribute the clips.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

CLIPS = {
    'launch.wav': 'project-pat-patttttaaaaaaaaaaa.mp3',
    'vvl.wav': 'bmw-bong.mp3',
    'four_cyl_honda.wav': 'asian-gong-music.mp3',
    'config_error.wav': 'you-stupid*.mp3',
    'boost_nitrous.wav': 'bombaclat-jamaican.mp3',
    'limiter_antilag.wav': 'minecraft-firework.mp3',
    'unsaved.wav': 'classic_hurt.mp3',
    'burning.wav': 'nom-nom-nom*.mp3',
    'burn_verified.wav': 'xp-level-up.mp3',
    'connected.wav': 'fart-with-reverb.mp3',
    'coolant_hot.wav': 'real-hot-girl-ish.mp3',
}


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('mp3_folder', type=Path, help='Directory containing the 11 supplied MP3 clips')
    p.add_argument('--output', type=Path, default=Path('sdcard/BrickECU/sounds'))
    a = p.parse_args()
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        print('ERROR: ffmpeg is required for MP3 to 22.05 kHz PCM WAV conversion.', file=sys.stderr)
        return 2
    source_dir = a.mp3_folder.expanduser().resolve()
    if not source_dir.is_dir():
        print(f'ERROR: Input directory not found: {source_dir}', file=sys.stderr)
        return 2
    a.output.mkdir(parents=True, exist_ok=True)
    missing = []
    for output_name, pattern in CLIPS.items():
        matches = sorted(source_dir.glob(pattern))
        if len(matches) != 1:
            missing.append(f'{pattern}: found {len(matches)}')
            continue
        source = matches[0]
        dest = a.output/output_name
        args = [ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-i', str(source),
                '-vn', '-ac', '1', '-ar', '22050', '-c:a', 'pcm_s16le', str(dest)]
        result = subprocess.run(args, capture_output=True, text=True)
        if result.returncode:
            print(f'ERROR converting {source.name}: {result.stderr}', file=sys.stderr)
            return 3
        print(f'{source.name} -> {dest} ({dest.stat().st_size:,} bytes)')
    if missing:
        print('Some clips could not be found uniquely:', file=sys.stderr)
        for item in missing:
            print('  '+item, file=sys.stderr)
        print('Copy all 11 original MP3s to one folder and run again.', file=sys.stderr)
        return 1
    print(f'Ready: copy {a.output.parent.parent} onto the 3DS SD card root.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
