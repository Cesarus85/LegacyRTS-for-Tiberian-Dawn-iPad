# Modern artwork asset rules

Modern artwork is an optional presentation layer. Original mode, simulation,
coordinates, hit testing, saves and network protocol must remain unchanged.
Every replacement must fall back independently to the original sprite when its
asset, renderer, animation state or visibility condition is unsupported.

## Directional source policy

- Deforming or articulated subjects such as infantry require at least five
  authored camera-fixed views: N, NE, E, SE and S. Mirror the latter three for
  SW, W and NW only when equipment and lighting are intentionally symmetric.
- Never build standing infantry facings by rotating a 2D bitmap. The character
  must turn around the vertical world axis while the camera stays fixed.
- A normal walk cycle needs at least three distinct phases plus the return
  phase. Prone, crawling, firing and transitions remain separate engine states.
- Rigid, nearly top-down vehicles may use deterministic planar rotation for an
  initial replacement. Tall cabins, visible side geometry, tracked vehicles,
  aircraft and all premium-quality revisions should use authored directional
  views because perspective, wheels/tracks and lighting change with facing.
- Independently rotating turrets remain separate assets with an explicit mount
  pivot. The pivot, not the asymmetric barrel bounds, defines rotation.

## Runtime contract

- Atlas order must match the engine facing order N, NE, E, SE, S, SW, W, NW.
- Generated atlases must validate dimensions, alpha and every required frame.
- World artwork, shadows, selection overlays and health bars are clipped to the
  tactical viewport, move by the same camera delta as the classic framebuffer,
  and obey local-player discovery/shroud state.
- Full tactical redraws and scene changes clear all registered world overlays.
  Modal game menus suspend the world-art layer and rebuild it after closing, so
  no unit, building, selection or health overlay can cover menu controls.
  Original mode bypasses the overlay layer entirely.
- Shared replacements ship and compile for iPadOS, macOS and visionOS together.

## Infrastructure and MCV atlas contract

The optional first infrastructure set adds separate GDI and Nod presentation
art without changing engine object types, placement cells or simulation:

- `building.infrastructure.gdi` and `building.infrastructure.nod` are RGBA
  768-by-256 atlases. Their three 256-pixel cells are Power Plant, faction
  infantry building (Barracks or Hand), and deployed Construction Yard.
- `unit.mcv.gdi` and `unit.mcv.nod` are RGBA 1536-by-192 atlases. Their eight
  192-pixel cells follow N, NE, E, SE, S, SW, W, NW.
- `building.infrastructure.activity.gdi` and
  `building.infrastructure.activity.nod` are RGBA 2560-by-768 atlases arranged
  as ten columns by three rows. Linear frames 0-19 are Construction-Yard
  production; frames 20-29 are Barracks/Hand activity. Runtime phase numbers
  come from the original building state machine, not presentation time.
- GDI/Nod selection follows the owning house's faction identity, including
  multiplayer houses; it must never depend only on a fixed campaign house ID.
- Building construction and deconstruction use a deterministic bottom-up reveal
  of the faction replacement, preserving the engine's real construction stage.
  Damage continuously darkens and weathers the same artwork; repairs retain the
  damaged state while adding a restrained repair indicator. This avoids abrupt
  switches between classic and modern styles. Unsupported special states still
  fall back independently to frame-correct original art.
- The Construction Yard remains modern while producing a building: its original
  20-frame active sequence selects 20 modern crane/work-light frames. GDI
  Barracks similarly retain their ten engine phases. Nod's original Hand has a
  one-frame active state, so its ten-frame door/light cycle is visual-only but
  derived from the shared deterministic simulation frame and changes no rules.
- The five authored activity poses are registered first and then expanded with
  eased interpolation. Thus every one of the 20 Construction-Yard phases and
  ten infantry-building phases changes smoothly instead of holding a pose and
  jumping to the next keyframe.
- All authored animation sheets use stable registration: moving parts may
  change the silhouette, while the fixed footprint keeps one scale, horizontal
  centre and baseline for the complete sequence. Building activity additionally
  inherits the corresponding static sprite's centre and baseline so entering
  or leaving an animation cannot jump. The deterministic builders reject
  footprint drift beyond one atlas pixel. Infantry poses use a common
  silhouette scale and centre; vehicle turning frames derive from one scaled
  master around a fixed pivot. Construction, damage and repair remain shader
  effects on the same registered sprite and therefore never rescale it.
- Static buildings pass local-player discovery and mapped-cell checks. Missing
  faction artwork falls back independently to the original sprite.
- Mobile replacements use the local player's current cell visibility, not the
  permanently explored/mapped bit. When their centre crosses the fog boundary,
  both the HD overlay and its hidden classic replacement are removed together;
  classic art can therefore never flash for one frame at the shroud edge.

Reproduction inputs and exact commands are kept beside the assets in
`resources/apple/ModernArt/units/infrastructure/source` and
`resources/apple/ModernArt/units/mcv/source`. The builders verify RGBA mode,
exact atlas dimensions, transparent padding and every required nonempty frame.

The current buggy and Humvee remain valid first-generation rigid-vehicle
replacements: their overhead silhouettes tolerate planar rotation reasonably
well. Before adding tanks, motorcycles, aircraft or another infantry class,
apply the directional policy above rather than copying the old one-view method.
