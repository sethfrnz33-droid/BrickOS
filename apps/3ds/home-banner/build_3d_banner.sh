#!/usr/bin/env bash
# Convert the mascot billboard GLB to a transparent 3DS CGFX HOME Menu banner.
# The model is not inserted into the Brick Tuner Suite app itself.
set -euo pipefail
cd "$(dirname "$0")/.."

# MSYS2's shell frequently cannot see a Windows Python installation even
# when the Windows Python Launcher (py.exe) is installed and working.
# Choose a real interpreter (not a Microsoft Store alias), Python >= 3.12.
PYTHON_CMD=()
try_python() {
  local output
  if output=$("$@" -c 'import sys; print(int(sys.version_info >= (3, 12)))' 2>/dev/null) && [[ "$output" == '1' ]]; then
    PYTHON_CMD=("$@")
    return 0
  fi
  return 1
}
find_python() {
  local candidate
  try_python python3.14 && return 0
  try_python python3.13 && return 0
  try_python python3.12 && return 0
  try_python python3 && return 0
  try_python python && return 0
  try_python py -3.14 && return 0
  try_python py -3.13 && return 0
  try_python py -3.12 && return 0
  try_python py -3 && return 0
  try_python py.exe -3.14 && return 0
  try_python py.exe -3.13 && return 0
  try_python py.exe -3.12 && return 0
  try_python py.exe -3 && return 0
  # Look for the standard per-user Windows Python installers if PATH hasn't
  # been reloaded into the MSYS2 shell after installation.
  local appdir=""
  if [[ -n "${LOCALAPPDATA:-}" ]] && command -v cygpath >/dev/null 2>&1; then
    appdir=$(cygpath -u "$LOCALAPPDATA")
  elif [[ -n "${USERPROFILE:-}" ]] && command -v cygpath >/dev/null 2>&1; then
    appdir="$(cygpath -u "$USERPROFILE")/AppData/Local"
  fi
  if [[ -n "$appdir" ]]; then
    for candidate in Python314 Python313 Python312; do
      try_python "$appdir/Programs/Python/$candidate/python.exe" && return 0
    done
    for candidate in 314 313 312; do
      try_python "$appdir/Programs/Python/Python$candidate/python.exe" && return 0
    done
    try_python "$appdir/Programs/Python/Launcher/py.exe" -3 && return 0
  fi
  if [[ -n "${WINDIR:-}" ]] && command -v cygpath >/dev/null 2>&1; then
    try_python "$(cygpath -u "$WINDIR")/py.exe" -3 && return 0
  fi
  return 1
}
if ! find_python; then
  cat >&2 <<'HELP'
ERROR: A usable Python 3.12+ was not found in this MSYS2 shell.
The .elf and .3dsx can compile without Python; the rotating 3D HOME Menu
banner requires Python to convert its GLB model into CGFX.

Install Python for Windows from https://www.python.org/downloads/windows/
(select Python 3.12 or newer, and enable "Add python.exe to PATH"), OR run
this command in Windows PowerShell if winget is available:
  winget install --id Python.Python.3.12 -e

Close/reopen the MSYS2 terminal after installing, then run:
  cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh

If Python is installed but not in PATH, the script also checks the Windows
py.exe launcher and standard per-user Python install directories.
No flat PNG replacement will be created: the real 3D banner is retained.
HELP
  exit 1
fi
printf 'Using Python: '
"${PYTHON_CMD[@]}" --version

# Isolated dependencies: avoid breaking MSYS2 or devkitPro's Python packages.
VENV="home-banner/.banner-venv"
PY="$VENV/bin/python"
if [[ -f "$VENV/Scripts/python.exe" ]]; then
  PY="$VENV/Scripts/python.exe"
fi
if [[ ! -f "$PY" ]]; then
  "${PYTHON_CMD[@]}" -m venv "$VENV" || {
    echo 'Python venv setup failed; confirm a complete Python 3.12+ installation.' >&2
    exit 1
  }
  if [[ -f "$VENV/Scripts/python.exe" ]]; then
    PY="$VENV/Scripts/python.exe"
  else
    PY="$VENV/bin/python"
  fi
fi
if ! "$PY" -c 'import gltflib; from PIL import Image' >/dev/null 2>&1; then
  echo 'Installing Python packages into the local HOME Menu banner environment...'
  "$PY" -m pip install --disable-pip-version-check gltflib Pillow
fi
mkdir -p home-banner/tools
if [[ ! -f home-banner/tools/pycgfx/main.py ]]; then
  if ! command -v git >/dev/null 2>&1; then
    echo 'Git is required for the first CGFX conversion. Install Git and retry.' >&2
    exit 1
  fi
  echo 'Fetching 3DS CGFX converter (first run only)...'
  git clone -q https://github.com/skyfloogle/pycgfx.git home-banner/tools/pycgfx
  git -C home-banner/tools/pycgfx checkout -q 1f78850086f3a77c41e07162e842f97a5bf3c18a
fi
# Mascot HOME Menu billboard only; never reintroduce an unrelated car.
BANNER_READY=0
for source in home-banner/mascot_banner.glb; do
  echo "Converting mascot HOME model: $source"
  rm -f home-banner/banner.cgfx
  if "$PY" home-banner/tools/pycgfx/main.py "$source" home-banner/banner.cgfx; then
    if "$PY" - "$source" <<'PYVALID'
from pathlib import Path
import sys
p=Path('home-banner/banner.cgfx')
source=sys.argv[1]
if not p.exists() or p.read_bytes()[:4] != b'CGFX':
    print(f'Conversion did not create valid CGFX for {source}',file=sys.stderr)
    sys.exit(1)
if p.stat().st_size > 0x80000:
    print(f'{source}: {p.stat().st_size:,} bytes exceeds CGFX 512 KiB limit',file=sys.stderr)
    sys.exit(1)
print(f'CGFX ready: {p} ({p.stat().st_size:,} bytes), from {source}')
PYVALID
    then
      printf '%s\n' "$source" > home-banner/banner_source.txt
      BANNER_READY=1
      break
    fi
  fi
done
if [[ "$BANNER_READY" != 1 ]]; then
  rm -f home-banner/banner.cgfx
  echo 'Mascot banner conversion failed. No unrelated fallback will be packaged.' >&2
  exit 1
fi
