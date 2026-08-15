# HD E1 Minigunner asset

This shared GDI/Nod replacement is newly created artwork and is not extracted
from commercial game data.

The original proof used one north-facing pose sheet and rotated its pixels in
the image plane. That is acceptable for a flat rigid object, but made a standing
soldier look prone at east/west facings. Version 2 uses five real views under a
fixed camera instead:

- `minigunner-master-n-chroma-v2.png`
- `minigunner-master-ne-chroma-v2.png`
- `minigunner-master-e-chroma-v2.png`
- `minigunner-master-se-chroma-v2.png`
- `minigunner-master-s-chroma-v2.png`

Each source was generated with OpenAI's built-in image generation tool using
the direction template in `PROMPT.md`. The deterministic builder removes the
magenta key, normalises each pose and mirrors SE/E/NE to produce SW/W/NW. It
never rotates a standing bitmap. Regenerate the 80-frame atlas with:

```sh
python3 tools/artwork/build_minigunner_atlas.py \
  --source-n resources/apple/ModernArt/units/minigunner/source/minigunner-master-n-chroma-v2.png \
  --source-ne resources/apple/ModernArt/units/minigunner/source/minigunner-master-ne-chroma-v2.png \
  --source-e resources/apple/ModernArt/units/minigunner/source/minigunner-master-e-chroma-v2.png \
  --source-se resources/apple/ModernArt/units/minigunner/source/minigunner-master-se-chroma-v2.png \
  --source-s resources/apple/ModernArt/units/minigunner/source/minigunner-master-s-chroma-v2.png \
  --output resources/apple/ModernArt/units/minigunner/minigunner-atlas-4x-v2.png
```

The renderer applies the owning house color at runtime, so GDI and Nod use one
maintainable source. Unsupported rare/death actions retain the original sprite
as a frame-correct fallback. The first-generation sources and atlas remain in
the tree as provenance, but the built-in manifest loads version 2.
