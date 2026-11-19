from pathlib import Path
root = Path(__file__).resolve().parents[1]
script = (root/'home-banner'/'build_3d_banner.sh').read_text()
assert (root/'home-banner'/'e46_gutted_original.glb').exists()
assert 'e46_home_banner_tiny_single.glb' in script
assert 'e46_home_banner_lite_single.glb' in script
assert 'find_python' in script
assert 'BLENDER_EXE' not in script
assert 'makebanner -ci home-banner/banner.cgfx' in (root/'build-cia.sh').read_text()
print('PASS: E46 prepared GLB wired as the HOME Menu 3D banner model')
