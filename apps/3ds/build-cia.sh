#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ ! -f BrickECU-3DS-Probe.elf ]]; then
  echo 'Missing ELF. Run make first.' >&2
  exit 1
fi
if [[ ! -f ./makerom.exe || ! -f ./bannertool.exe ]]; then
  echo 'Put makerom.exe and bannertool.exe in this project folder.' >&2
  exit 1
fi
if [[ ! -f project.rsf ]]; then
  echo 'Retrieving 3DS homebrew RSF template...'
  curl -fL --retry 2 'https://raw.githubusercontent.com/Dogbonee/3DS_Homebrew_Template/main/project.rsf' -o project.rsf
  # Change template identity and ensure Old 3DS hardware-compatible settings.
  sed -i 's/Cia Builder Example/Brick Tuner Suite/g; s/CTR-P-PWDR/CTR-P-BEPR/g; s/0xFAA5C/0xFBE31/g; s/804MHz/268MHz/g; s/EnableL2Cache *: true/EnableL2Cache     : false/g; s/CanAccessCore2 *: true/CanAccessCore2    : false/g' project.rsf
  sed -i '/^RomFs:/,+2d' project.rsf
fi
# Keep the original Title ID for seamless home-menu upgrades.
if [[ -f project.rsf ]]; then sed -i 's/Brick ECU Field Guide/Brick Tuner Suite/g' project.rsf; fi
# The HOME Menu banner is a real 3D E46 with a rigid rotating model node.
# Regenerate when its model or banner converter has changed.
# Existing v4.8 banner.cgfx cannot be reused after the v4.9 model update.
# A successful v4.9 build records the exact accepted source for caching.
MODEL_CHANGED=0
for source in home-banner/e46_diorama_full.glb home-banner/e46_diorama_tiny.glb home-banner/e46_diorama_lean.glb home-banner/e46_diorama_safe.glb; do
  if [[ "$source" -nt home-banner/banner.cgfx ]]; then MODEL_CHANGED=1; fi
done
if [[ ! -s home-banner/banner.cgfx || ! -f home-banner/banner_source.txt ]] ||
   ! grep -q '^home-banner/e46_diorama_' home-banner/banner_source.txt ||
   [[ "$MODEL_CHANGED" == 1 ]]; then
  echo 'Preparing rotating E46 + 3DS HOME Menu banner...'
  bash home-banner/build_3d_banner.sh
fi
./bannertool.exe makebanner -ci home-banner/banner.cgfx -a banner.wav -o banner.bnr
./bannertool.exe makesmdh -s 'Brick Tuner Suite' -l 'Brick Tuner Suite - Brick ECU' -p 'Brick ECU' -f 'visible,allow3d,recordusage,extendedbanner' -i icon.png -o icon.icn
./makerom.exe -f cia -o BrickECU-3DS-Probe.cia -rsf project.rsf -target t -exefslogo -elf BrickECU-3DS-Probe.elf -icon icon.icn -banner banner.bnr
printf '\nCIA created: %s\n' "$PWD/BrickECU-3DS-Probe.cia"
