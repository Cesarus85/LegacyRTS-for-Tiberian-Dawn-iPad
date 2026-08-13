# HD E1 Minigunner asset

This shared GDI/Nod replacement is newly created artwork and is not extracted
from commercial game data.

- `minigunner-master-chroma.png` was generated with OpenAI's built-in image
  generation tool using `PROMPT.md`.
- `minigunner-master-transparent.png` is the locally chroma-keyed source.
- `../../minigunner-atlas-4x.png` is generated deterministically with
  `tools/artwork/build_minigunner_atlas.py`.

The atlas contains ten gameplay poses in all eight engine facings (80 frames).
The renderer applies the owning house color at runtime, so GDI and Nod use one
maintainable source. Unsupported rare/death actions retain the original sprite
as a frame-correct fallback.
