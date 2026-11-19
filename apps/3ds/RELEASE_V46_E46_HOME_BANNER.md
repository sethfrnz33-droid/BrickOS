# Brick Tuner Suite v4.6 — E46 HOME Menu 3D banner

- Replaced the rotating HOME Menu 3D banner source with the user's uploaded `e46 low poly.blend`.
- Added automatic Blender -> GLB export in `home-banner/build_3d_banner.sh`.
- Updated the build pipeline so the CIA packaging step rebuilds the banner when the Blender source changes.
- Left the tuning application UI unchanged.

## Verification
- Host-side banner-script checks passed for file paths and build-script wiring.
- Actual Blender export, CGFX conversion, and 3DS HOME Menu rendering remain unverified in this environment.
