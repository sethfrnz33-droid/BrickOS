# Test report — v3.9

- **Native direct-3D INI test:** `[TableEditor]` 2D id `veTableTbl` resolves to one `veTable` field in 2D; alternate `veTableMap` resolves to the *same* field in 3D. Indirect `veTableDialog3D` panel also resolves to 3D.
- **Native nonzero data test:** 4×4 U16 test map with values 50..200 scaled by 0.5 decodes 25..100 without inventing zeros, using the same source `get_number` and `field_value` functions used for 2D and 3D.
- **Native gauge de-duplication test:** verifies neither name nor ECU offset/type repeats top-screen home channels, and tune fallback excludes RPM/CLT/AFR.
- **UI regression:** all preexisting test suites still pass with their obsolete 3D-toggle coordinates/expectations updated to match the deliberately changed interaction.
- **Limitations:** device compilation, rendering, network response and real ECU data have not been tested here. A genuinely all-zero ECU table will continue to display zeros (now explicitly identified as flat), not manufactured values.
