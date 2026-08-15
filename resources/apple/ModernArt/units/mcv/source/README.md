# HD Mobile Construction Vehicle source sheets

These are newly generated GDI and Nod Mobile Construction Vehicle replacement
graphics, not extracts from commercial game data. Each source contains five
real, camera-fixed directional views. The generation request used the order
N, NE, E, SE, S, but visual inspection of the cab/front found that the image
generator interpreted those camera labels as the opposite world headings:
S, SW, W, NW, N.

- `gdi-mcv-directions-generated-v1.png` matches the warm concrete/gold
  infrastructure set.
- `nod-mcv-directions-generated-v1.png` matches the charcoal/red set.
- `PROMPT.md` preserves the reproducible generation request.

The deterministic builder crops the five equal source slots, uses one common
scale for stable turning, normalises that source-specific 180-degree heading
offset, and mirrors compatible views for the remaining directions. It
preserves the generated contact shadows and never rotates the vehicle bitmap
in-plane:

```sh
python3 tools/artwork/build_mcv_atlas.py \
  --source resources/apple/ModernArt/units/mcv/source/gdi-mcv-directions-generated-v1.png \
  --output resources/apple/ModernArt/units/mcv/gdi-mcv-atlas-4x.png

python3 tools/artwork/build_mcv_atlas.py \
  --source resources/apple/ModernArt/units/mcv/source/nod-mcv-directions-generated-v1.png \
  --output resources/apple/ModernArt/units/mcv/nod-mcv-atlas-4x.png
```

Each output is RGBA, 1536 by 192 pixels, with eight nonempty transparent cells
in engine facing order N, NE, E, SE, S, SW, W, NW.
