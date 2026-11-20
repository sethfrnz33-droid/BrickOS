#!/usr/bin/env python3
"""Regenerate main/web_assets.c from the BrickOS web directory.

Makes symbol definitions independent of ESP-IDF's EMBED_TXTFILES path
mangling, which differs when supplied paths contain subdirectories.
"""
from pathlib import Path

root = Path(__file__).resolve().parent.parent
assets = {
    'html': 'index.html',
    'css': 'app.css',
    'js': 'app.js',
    'core': 'core.mjs',
    'ini_fast_cache': 'ini-fast-cache.mjs',
    'guided': 'guided.mjs',
    'guided_ui': 'guided-ui.mjs',
    'guide_story': 'guide-story.mjs',
    'guide_reasoning': 'guide-reasoning.mjs',
    'guide_story_ui': 'guide-story-ui.mjs',
    'guide_voice': 'guide-voice.mjs',
    'seth_page': 'seth.html',
    'guide_catalog': 'guide-catalog.json',
    'seth_head': 'guide-assets/seth-head.png',
    'seth_jaw': 'guide-assets/seth-jaw.png',
    'seth_mouth': 'guide-assets/seth-mouth.png',
    'seth_voice': 'guide-assets/voice.mp3',
}
header = root / 'main' / 'web_assets.h'
source = root / 'main' / 'web_assets.c'

header_text = '''// Generated webpage symbols with stable C names. Re-run tools/generate_web_assets.py after web edits.
#pragma once
#include <stddef.h>
#include <stdint.h>
'''
source_text = '''// Generated from web/* by tools/generate_web_assets.py; do not edit by hand.
#include "web_assets.h"
'''
for name, file_name in assets.items():
    data = (root / 'web' / file_name).read_bytes()
    header_text += f'extern const uint8_t brick_asset_{name}[];\nextern const size_t brick_asset_{name}_len;\n'
    # Include a NUL for convenience, while HTTP content length excludes it.
    vals = data + b'\x00'
    chunks = [vals[i:i+16] for i in range(0, len(vals), 16)]
    source_text += f'\nconst uint8_t brick_asset_{name}[] = {{\n'
    source_text += ''.join('    ' + ', '.join('0x%02x' % b for b in chunk) + ',\n' for chunk in chunks)
    source_text += '};\n'
    source_text += f'const size_t brick_asset_{name}_len = sizeof(brick_asset_{name}) - 1;\n'

header.write_text(header_text, encoding='utf-8')
source.write_text(source_text, encoding='utf-8')
print('Generated web_assets.c from web files:', source.stat().st_size, 'bytes')
