# HD buggy proof asset

This is newly created replacement artwork for the iPadOS proof pack; it is not
extracted from the commercial Command & Conquer data.

- `buggy-master-chroma.png` was generated with OpenAI's built-in image generation
  tool from the production prompt preserved in `PROMPT.md`.
- `buggy-master-transparent.png` is the locally chroma-keyed source.
- `../../buggy-atlas-4x.png` is deterministically generated from the transparent
source by `tools/artwork/build_buggy_atlas.py`.

The builder centers the turret on its circular mounting ring rather than on the
asymmetric visible bounds of the long machine gun, so every rotated facing
stays fixed to the Buggy socket.

The atlas contains 64 cells in an 8-by-8 layout. Cells 0–31 are chassis facings;
cells 32–63 are independently rotating turret facings. Every cell is 96 pixels,
representing the engine's 24-pixel logical footprint at 4x scale.
