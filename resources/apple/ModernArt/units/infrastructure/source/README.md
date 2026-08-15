# HD infrastructure source sheets

These are newly generated replacement graphics, not extracts from commercial
game data. Each faction sheet contains three equal horizontal source slots in
runtime order: Power Plant, infantry production building, and deployed
Construction Yard.

- `gdi-infrastructure-master-generated-v1.png` is the warm concrete, steel and
  gold faction set.
- `nod-infrastructure-master-generated-v1.png` is the dark steel and red
  faction set.
- `PROMPT.md` preserves the reproducible generation request.

Both generated PNGs already contain useful soft alpha around the objects and
their contact shadows. The deterministic builder removes invisible dark matte
pixels, normalises the remaining alpha, crops each equal source slot, and fits
it into a transparent 256-pixel cell:

```sh
python3 tools/artwork/build_infrastructure_atlas.py \
  --source resources/apple/ModernArt/units/infrastructure/source/gdi-infrastructure-master-generated-v1.png \
  --output resources/apple/ModernArt/units/infrastructure/gdi-infrastructure-atlas-4x.png

python3 tools/artwork/build_infrastructure_atlas.py \
  --source resources/apple/ModernArt/units/infrastructure/source/nod-infrastructure-master-generated-v1.png \
  --output resources/apple/ModernArt/units/infrastructure/nod-infrastructure-atlas-4x.png
```

Each output is RGBA, 768 by 256 pixels, with three nonempty transparent cells.
The renderer chooses faction and destination footprint; the original art stays
the fallback for unsupported animation and shroud state.

Production activity uses four additional five-keyframe source strips. The
shared builder removes generated dark/checker mattes, aligns the fixed camera
and footprint, then expands the authored poses into a 10-by-3 runtime atlas:

```sh
python3 tools/artwork/build_infrastructure_activity_atlas.py \
  --conyard-source resources/apple/ModernArt/units/infrastructure/source/gdi-conyard-active-keyframes-generated-v1.png \
  --infantry-source resources/apple/ModernArt/units/infrastructure/source/gdi-barracks-active-keyframes-generated-v1.png \
  --reference-atlas resources/apple/ModernArt/units/infrastructure/gdi-infrastructure-atlas-4x.png \
  --output resources/apple/ModernArt/units/infrastructure/gdi-infrastructure-activity-atlas-4x.png

python3 tools/artwork/build_infrastructure_activity_atlas.py \
  --conyard-source resources/apple/ModernArt/units/infrastructure/source/nod-conyard-active-keyframes-generated-v1.png \
  --infantry-source resources/apple/ModernArt/units/infrastructure/source/nod-hand-active-keyframes-generated-v1.png \
  --reference-atlas resources/apple/ModernArt/units/infrastructure/nod-infrastructure-atlas-4x.png \
  --output resources/apple/ModernArt/units/infrastructure/nod-infrastructure-activity-atlas-4x.png
```

Frames 0-19 are a ping-pong Construction-Yard crane cycle. Frames 20-29
contain the corresponding infantry-production door/light activity. Registered
neighbouring keyframes are expanded with eased interpolation, producing a
distinct visual phase for every engine frame instead of holding and jumping
between the five authored poses.
