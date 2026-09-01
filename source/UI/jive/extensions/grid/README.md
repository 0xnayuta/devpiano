# JIVE Grid Extension (Strategic Asset - KEEP-LATER)

Pursuant to **ADR-014**, the Grid layout capability (`GridContainer`, `GridItem`, and `GridVariantConverters`) is preserved here as a strategic extension asset.

- **Status**: Reserved / KEEP-LATER (not compiled into Phase 1 default build).
- **Future Product Use Cases**:
  1. Preset Browser multi-column card grid / waterfall layout.
  2. MIDI / Channel Matrix 16-channel cross-routing matrix panel.
  3. Advanced keyboard zone split / layer parameter alignment tables.
- **Activation Path**:
  To activate Grid support in the future:
  1. Add `#define JIVE_ENABLE_GRID 1` in configuration.
  2. Add `source/UI/jive/extensions/grid/*.cpp` to `CMakeLists.txt` build targets.
  3. Include `jive_GridContainer.h` and `jive_GridItem.h`.
