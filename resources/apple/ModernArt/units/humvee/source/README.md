# HD Humvee asset

This is newly created replacement artwork for the iPadOS HD pack and is not
extracted from commercial game data.

- `humvee-master-chroma.png` was generated with OpenAI's built-in image
  generation tool using `PROMPT.md`.
- `humvee-master-transparent.png` is the locally chroma-keyed source.
- `../../humvee-atlas-4x.png` is generated deterministically with
  `tools/artwork/build_buggy_atlas.py --turret-pivot-y 0.681`.

The 8-by-8 atlas contains 32 chassis and 32 independently rotating turret
facings. The explicit mounting-ring pivot prevents the asymmetric gun barrel
from shifting the turret off its socket.
