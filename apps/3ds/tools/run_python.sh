#!/usr/bin/env bash
# Portable Python launcher for optional mascot asset regeneration.
# Normal ZIP builds already contain the artwork and never call this script.
set -euo pipefail
if [[ $# -lt 1 ]]; then echo 'Usage: bash tools/run_python.sh SCRIPT.py [args...]' >&2; exit 2; fi
script=$1; shift
run_with() {
  local status
  if status=$("$@" -c 'import sys;print(int(sys.version_info >= (3, 8)))' 2>/dev/null) && [[ "$status" == 1 ]]; then
    echo "Using Python: $*"
    "$@" "$script" "${script_args[@]}"
    exit $?  # Found a real interpreter; preserve generation errors verbatim.
  fi
  return 1
}
script_args=("$@")
run_with python3 || run_with python || run_with python3.exe || \
run_with py -3 || run_with py.exe -3 || {
  appdir=""
  if [[ -n "${LOCALAPPDATA:-}" ]] && command -v cygpath >/dev/null 2>&1; then
    appdir=$(cygpath -u "$LOCALAPPDATA")
  elif [[ -n "${USERPROFILE:-}" ]] && command -v cygpath >/dev/null 2>&1; then
    appdir="$(cygpath -u "$USERPROFILE")/AppData/Local"
  fi
  found=1
  if [[ -n "$appdir" ]]; then
    for folder in Python314 Python313 Python312 Python311 Python310 Python39 Python38; do
      if run_with "$appdir/Programs/Python/$folder/python.exe"; then found=0; break; fi
    done
  fi
  if [[ "$found" -ne 0 ]] && [[ -n "${WINDIR:-}" ]] && command -v cygpath >/dev/null 2>&1; then
    run_with "$(cygpath -u "$WINDIR")/py.exe" -3 && found=0
  fi
  if [[ "$found" -ne 0 ]]; then
    echo 'ERROR: Mascot source files missing and Python not found.' >&2
    echo 'Use the complete Brick Tuner Suite ZIP (contains all generated art) or install Python for Windows.' >&2
    exit 127
  fi
}
