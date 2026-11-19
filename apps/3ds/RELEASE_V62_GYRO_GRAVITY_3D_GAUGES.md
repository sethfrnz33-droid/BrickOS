# Brick Tuner Suite v6.2 — Motion-aware rounded 3D gauges

- Main dashboard digital gauges now use procedurally generated **rounded 3D prisms**: front face, curved edge/rim, sidewalls and a shadow; no stretched corner textures.
- The panel geometry uses fixed pixel corner radii so all gauge layouts, from one large panel to nine smaller panels, retain their rounded edges.
- Gauge readings remain normal, centered, sharp 2D text drawn on the face with the same stereo displacement, preventing separated/double-looking text.
- **3DS accelerometer:** gravity-based tilt displaces each floating gauge toward a stable equilibrium.
- **3DS gyroscope:** angular motion supplies damped impulses, so the cards swing and settle instead of following the device rigidly.
- Each gauge has an independent gentle, slow phase offset when held still; motion is capped to retain readability and avoid overlaps.
- The larger boxes on the top tuning screen also use the universal rounded prism; the touchscreen controls remain flat and hit targets unchanged.
- The former highlighted RPM gauge on the home dashboard is now visually equal to the other gauges: no false selection state.
- A sensor failure gracefully falls back to subtle idle motion. Motion is stepped **once per rendered frame**, never once per eye, to keep both stereoscopic views in phase.
- No changes to ECU transport, calibration, log deletion, sound triggers, or write/burn safeguards.

## Hardware caveat
The rendering/sensor integration compiles with host stubs and passes native motion/safety tests, but **real 3DS sensor orientation, stereo perception and frame rate are not verified here**. If a device axis feels reversed on your console, that must be verified and adjusted in a hardware iteration.

## Build
```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
