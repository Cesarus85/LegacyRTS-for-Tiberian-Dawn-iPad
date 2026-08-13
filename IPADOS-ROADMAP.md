# Tiberian Dawn iPadOS optimization roadmap

This document records the iPad-specific audit and implementation work. The
physical iPad build is now the sole development and acceptance target; the
simulator build is retained only as an unused legacy fallback. Items below have
not been implemented unless the current status explicitly says otherwise.

## August 2026 production pass

Implemented in the canonical physical-iPad target:

- Guided first-launch import accepts the GDI and Nod Gold ISOs together or an
  already prepared directory. ISO-9660 and `INSTALL/SETUP.Z` are extracted
  without retaining ISO copies; the complete result is validated in a staging
  directory and committed atomically after a 1.2 GB free-space check.
- A native, scrollable first-launch guide explains legal data ownership,
  supported editions, exact ISO names, Files/iCloud/USB preparation, joint
  multi-selection, storage, privacy and prepared-folder import. Dynamic Type,
  VoiceOver hints, visible progress, retry help, completion confirmation and an
  explicit set-up-later path are included.
- Immutable assets live in
  `Library/Application Support/TiberianDawn/vanillatd` and are excluded from
  backup. Settings, manual saves, and recovery saves remain in the
  Files-visible `Documents/TiberianDawn/vanillatd` directory.
- `AVAudioSession` uses the system-silent-switch policy and observes audio
  interruptions, route/Bluetooth changes, and media-service resets.
- Visual Controls offers full-size sharp, integer pixel-exact, or classic
  nearest-neighbour rendering, 100/125/150 percent touch UI, and a
  large-cursor/high-contrast mode. Pinch also adjusts the touch UI scale
  directly.
- Compact Stage Manager windows show an accessible minimum-size warning.
- A custom original AppIcon, launch color, privacy manifest, and signed
  `ipados-device-release` preset are part of the Xcode target.
- Multiplayer is visibly unavailable and disabled while networking is omitted.
  Red Alert reuse is prepared through the shared `platform/apple` layer, but
  enabling and validating the separate game target remains its own milestone.
- German and English localization is centralized for every iPad-specific menu,
  import/save dialog, recovery prompt and compact-window warning. The persistent
  language setting follows the iPad by default, supports explicit German or
  English, and safely falls back to the original English engine text table.

True tactical zoom, network transport, and full framebuffer semantics for
VoiceOver remain separate engine-scale projects rather than release blockers
for the single-player iPad port.

The next implementation milestones are explicitly prioritized:

1. Physical visual acceptance of all three Metal presentation modes across
   menus, missions, palette fades, cursor/touch feedback, and Stage Manager.
2. Physical audio acceptance across music, speech, sound effects, movies,
   interruption recovery, and route changes.
3. Physical acceptance of the new native save transfer workflow.

Physical-device fault injection that requires manual interaction or actually
exhausting device storage remains a later acceptance pass and does not block
these milestones.

## Current status

- VanillaTD builds, signs, installs, and launches on the physical iPad.
- A custom indexed-palette Metal path renders the original 8-bit framebuffer
  and 256-color palette directly with VSync; the SDL 32-bit path is retained as
  an automatic fallback.
- The complete GDI and Nod C&C Gold data set is recognized.
- The main menu renders and one-finger selection works.
- UIKit uses the scene lifecycle and supports all four iPad orientations.
- The game resizes dynamically, respects safe areas, and preserves its original
  16:10 aspect ratio without stretching.
- Touch, mouse, cursor, and hit testing use the same recalculated coordinate
  transform after every drawable-size change.
- Active single-player missions are protected by transactional rotating
  recovery autosaves when the app enters the background.
- A valid interrupted mission is offered through a Continue prompt on the next
  launch.
- Game assets are stored below `Library/Application Support/TiberianDawn/vanillatd`;
  settings and saves remain below `Documents/TiberianDawn/vanillatd`.
- Touch input distinguishes tap, thresholded selection drag, long press,
  two-finger pan, two-finger secondary click, and cancellation.
- Native text input opens the iPad keyboard for edit fields and respects the
  active hardware-keyboard layout, including German umlauts and sharp S.
- Mouse/trackpad absolute hover, clicks, secondary buttons, and wheel input use
  the same safe-area-aware transform as touch.
- Controllers are detected and reconnected automatically, with a connected-
  controller-specific layout guide in Game Controls.
- Finger and Pencil input has visible press/hover feedback; supported Pencils
  also provide hover, double-tap secondary click, and squeeze-to-go-back.
- Networking and the Red Alert target are disabled in the iPad presets.
- GDI and Nod disc folders are detected on the case-sensitive physical-device
  filesystem without lowercasing iOS container paths.
- Main-menu `Exit Game` performs the normal settings/audio/video shutdown and
  then terminates the iPad process directly instead of leaving a black window.

## Recommended implementation order

1. Dynamic iPadOS window sizing, orientation, and coordinate transforms.
2. Lifecycle handling, autosave, and safe interruption recovery.
3. Native RTS touch gestures and cancellation handling.
4. Frame pacing, idle rendering, and battery optimization.
5. On-screen keyboard, hardware keyboard, mouse, and trackpad refinements.
6. Guided on-device import of legally supplied original game data.
7. Modern audio session and audio backend work.
8. Metal palette rendering, visual polish, and accessibility.
9. Multiplayer decision and Red Alert reuse.

## P0: Dynamic windows, orientation, and layout

The first adaptive-windowing milestone is implemented. The app no longer
requires the deprecated full-screen compatibility mode, and its render and
input layout now reacts to UIKit scene and SDL drawable-size changes.

Implemented:

- UIKit scene lifecycle on iPadOS 15 and newer.
- All portrait and landscape orientations, without `UIRequiresFullScreen`.
- SDL window resized, size changed, display changed, and orientation
  events.
- Recalculation of the render destination and all input transforms after every
  drawable-size change.
- One shared transform for touch, mouse, cursor, and hit testing.
- Safe-area-aware centering around the Home indicator and iPad window controls.
- Centering of the original 16:10 game surface without distortion.
- A deterministic geometry matrix covers landscape, portrait, compact Stage
  Manager sizes, asymmetric safe areas, Retina point/pixel conversion,
  pixel-perfect fallback and ultrawide external displays.
- Pixel-perfect mode falls back to a contained fractional fit when no complete
  integer 640-by-400 surface fits, instead of drawing outside a tiny window.

Remaining work:

- Evaluate SDL 3.4+ or `sdl2-compat` because the vendored SDL 2.32.10 backend
  still uses the legacy UIKit application lifecycle.
- Deferred: complete the external-display device test matrix in a later pass;
  it is not part of the current acceptance scope.

## P0: Lifecycle and autosave

The first interruption-recovery milestone is implemented. UIKit keeps a short
background execution task alive while the engine performs its synchronous save
on the game thread.

Implemented:

- An SDL event watch captures lifecycle events immediately while atomic flags
  defer all engine work to the normal game-thread event pump.
- Duplicate UIKit/SDL will/did notifications collapse into one background or
  foreground transition.
- Backgrounding releases a held primary touch, suspends music and movie audio,
  and stops simulation and rendering work.
- Settings are flushed and synchronized before suspension.
- Mission state is first written to `autosave.ipad.tmp`, synchronized, then
  atomically renamed into the older of `autosave.ipad0` and
  `autosave.ipad1`. The containing directory is synchronized after commit.
- The recovery commit is shared with a deterministic host test that injects a
  partial writer/ENOSPC-style failure, a thrown writer, an empty output and
  rotation across both existing slots. A failed file synchronization can no
  longer be mistaken for a committed recovery save.
- Incomplete temporary saves are removed and never offered for recovery.
- A valid newest slot produces an Interrupted Mission/Continue prompt after
  relaunch. Recovery slots are removed after the mission ends or the player
  declines recovery.
- A memory warning releases reconstructible interpolated-palette caches when no
  movie is active.
- Duplicate lifecycle notices collapse, while both edges of a rapid
  background/foreground cycle are retained until the engine consumes them, so
  the background autosave cannot be overwritten before the next game tick.
- A deterministic lifecycle test covers duplicates, rapid cycles, termination,
  low-memory delivery, foreground state and handler dispatch.
- Physical recovery stress testing covers normal app-switch termination and an
  explicit uncatchable SIGKILL. Two background saves rotate through
  `autosave.ipad0` and `autosave.ipad1` without leaving a temporary file; the
  newest slot restores the running mission after relaunch; and returning to the
  main menu removes both recovery slots while preserving manual saves.

Remaining work:

- Validate a genuine low-storage autosave failure on a physical iPad.
- Expand safe memory-pressure cleanup after profiling which additional caches
  can be reconstructed without destabilizing a running mission.

## P0: Native RTS touch controls

The core native RTS gesture milestone is implemented. Touch-generated SDL mouse
events are disabled so every direct touch is interpreted exactly once by the
iPad gesture state machine, while real mouse and trackpad events keep the
classic desktop path.

Implemented default control scheme:

- Tap: select a unit or issue the normal context-sensitive command.
- One-finger drag: begin a selection rectangle only after a 12-game-pixel
  movement threshold, preventing hand jitter from turning taps into drags.
- Two-finger pan: move the tactical camera.
- Long press: secondary click, cancel, or deselect after 500 milliseconds.
- Two-finger tap: alternative secondary click.
- A third simultaneous touch safely cancels the current gesture.
- Adding a second finger during selection releases the held primary button
  before switching to camera pan.
- UIKit touch cancellation is preserved through SDL and never converted into
  an accidental tap.
- Focus loss and backgrounding release held buttons and discard unfinished
  gestures and stale pan deltas.
- Apple Pencil follows the precise primary-touch selection path.

Remaining work:

- Pinch: initially adjust presentation or UI scale; true tactical zoom is a
  separate engine project because the classic viewport assumes a fixed scale.
- Ignore or coordinate with reserved iPad system gestures.
- Consider an optional, unobtrusive command overlay for frequent commands.
- Validate two-finger pan, long press, cancellation, Pencil, and system-gesture
  conflicts on a physical iPad. Tap selection and thresholded drag have been
  exercised in a live GDI mission in the simulator.

## P1: Performance, frame pacing, and battery

Implemented for the physical iPad build:

- Normal presentation is capped at 60 FPS without changing the historical
  simulation tick rate.
- `Game Controls` contains a persistent `Battery mode: ON/OFF` control. Battery
  mode caps active presentation at 30 FPS.
- iPadOS Low Power Mode and serious or critical thermal pressure automatically
  cap active presentation at 30 FPS, independent of the manual setting.
- Static output falls back to a 15 Hz dirty check. Byte-identical indexed
  frames skip palette conversion, the full texture upload, render copy, and
  present entirely.
- Palette and software-cursor changes participate in dirty detection, so fades,
  animated cursors, and pointer movement are not lost.
- Backgrounded, hidden, minimized, or inactive windows do not present. The
  first foreground frame is forced after resume or a layout change.
- The Debug device build reports five-second physical-device telemetry to the
  Xcode console: target and actual FPS, texture-upload rate, dirty and inactive
  skips, average present time, process CPU, resident memory, average/maximum
  touch-to-present latency, Low Power Mode, and thermal state.

Physical-device validation matrix:

- Build, code signing, update installation, and launch: automated with the
  `ipados-device` preset and `devicectl`.
- Static menu, live mission, battery-mode transition, app switching, movies,
  screen recording, rapid suspend/resume, and a long-mission soak are checked
  against the telemetry stream. Record any visual discontinuity, audio issue,
  memory growth, thermal escalation, or touch-latency spike before release.
- Instruments Energy Log remains the release authority for system-wide energy
  impact; the in-app upload/present/CPU counters make regressions reproducible
  during ordinary Xcode runs.

## P1: Keyboard, mouse, trackpad, controller, and Pencil

Implemented for the physical iPad build:

- Open the iPad on-screen keyboard when an engine edit field gains focus.
- Consume UTF-8 text input events for characters while retaining physical key
  events for commands. The legacy 8-bit font path supports Latin-1, including
  German Ä/Ö/Ü/ä/ö/ü/ß; unsupported Unicode is safely ignored or folded.
- Respect the active software and hardware keyboard layout instead of using the
  fixed US scancode table for text.
- Correct absolute mouse coordinates when the game is letterboxed.
- Preserve precise mouse and trackpad clicks, wheel scrolling, hover, and
  secondary buttons.
- Use the vendored SDL UIKit `GCMouse` path and indirect-pointer events while
  filtering synthesized touch-mouse duplicates.
- Leave idle presentation cadence immediately on input, removing the extra
  first-event delay after a static screen.
- Auto-detect, hot-plug, disconnect, and reconnect supported controllers with
  no hidden INI switch.
- Show Xbox-style, PlayStation, or Nintendo labels in the new `Controller
  layout` item under Game Controls.
- Provide left-stick pointer control, right-stick map pan, primary/secondary
  action buttons, guard/formation buttons, modifiers, team shortcuts, and
  trigger speed boost through the standard SDL controller mapping.
- Draw yellow finger feedback and blue Pencil feedback without modifying the
  original indexed framebuffer.
- Give classic controls a minimum 24-by-24 logical-pixel finger target (about
  44 iPad points at the usual presentation size) without enlarging artwork.
- Use the tighter four-pixel drag threshold for Pencil, supported Pencil hover
  as target preview, double-tap as secondary click, and Pencil Pro squeeze as
  Escape/back.

Remaining validation:

- Exercise German software/hardware keyboards, mouse, trackpad, representative
  Xbox/PlayStation/Nintendo controllers, and each supported Pencil generation
  on physical hardware.

## P1: Guided game-data import and storage

Implemented and verified on the physical iPad:

- A native first-launch guide and system document picker accept the GDI and Nod
  C&C Gold ISOs together or an already prepared data directory.
- The importer validates the disc structure and required MIX files, extracts
  `INSTALL/SETUP.Z`, checks free space, reports progress and commits through a
  verified staging directory.
- ISO extents, directory records, archive offsets, decompressed sizes and path
  components are range checked before allocation or extraction. Partial output
  is removed, `SETUP.Z` stays inside the disposable staging directory, and
  required MIX entries must be non-empty regular files.
- Deterministic tests cover truncated/malformed ISO records, invalid file
  extents, corrupt InstallShield headers and records, insufficient/unknown
  capacity, normal staging transitions, failed replacement and rollback. The
  stricter parser also extracts representative data from both real Gold ISOs.
- Immutable assets are kept in `Library/Application Support`; settings and
  saves remain separate and Files-visible in Documents.
- The isolated acceptance test covers set-up later, a missing-disc error,
  successful dual-ISO import, completion, game startup, and a subsequent launch
  that skips the importer.
- The picker explicitly supports iPadOS's extension-derived dynamic `.iso`
  type. Proprietary original game data is never bundled with the app.
- A native `Spielstände / Dateien` manager is available both from the main
  menu and from Game Controls during a mission. It lists only manual saves and
  explicitly excludes recovery slots and settings.
- Valid saves can be exported individually or together as descriptively named
  `.cncsave` documents through the system picker to iCloud Drive, On My iPad,
  USB storage or another Files provider.
- One or several `.cncsave` or original `SAVEGAME` files can be imported. Each
  file is size- and version-validated, synchronized and atomically committed to
  the first free `SAVEGAME.###` slot; existing saves are never overwritten.
- The app declares its `.cncsave` document type, reports per-file errors for a
  mixed selection, removes temporary copies, and caps imports at 128 MB.
- A host policy matrix covers exact manual-save recognition, recovery-file
  exclusion, slot exhaustion, path sanitization and deterministic export names.

Remaining work:

- Physically exercise export to iCloud Drive and import the resulting
  `.cncsave` file back into a free slot, then load it through the classic menu.
- Later physical acceptance only: repeat corrupt/truncated ISO, corrupt
  `SETUP.Z`, and genuine low-storage failures through the visible iPad UI.

## P2: Audio modernization

The iPad target now uses Apple's native AVAudioEngine and no longer compiles or
links the deprecated OpenAL backend. Desktop targets retain their existing
OpenAL implementation.

Implemented:

- Configure an appropriate `AVAudioSession` category for a game.
- Respect the system mute policy selected for the app.
- Handle Siri, alarms, audio interruptions, route changes, Bluetooth devices,
  and media-service resets.
- Feed music, speech, sound effects and VQA movie audio through independent
  `AVAudioPlayerNode` streams connected to one shared native mixer.
- Rebuild active streams after a media-service reset and resume the engine on
  foreground and route-change events.
- Track the two-buffer streaming queues independently of completion callbacks;
  stale callbacks from stopped/rebuilt streams cannot corrupt current state.
- Cover queue saturation, draining, pause and reset generations with an
  automated backend-state test.

## P2: Rendering and presentation quality

Implemented:

- Render the 640-by-400 indexed framebuffer with a custom Metal pipeline using
  an 8-bit index texture and a separate 256-color palette texture.
- Process palette animation and fades on the GPU without reconverting or
  re-uploading the framebuffer.
- Upload only the bounding rectangle of changed indices; unchanged scenes skip
  both upload and presentation. Even a full-frame framebuffer upload is 75
  percent smaller than the former 32-bit SDL texture upload.
- Offer three bilingual presentation modes in Visual Controls: recommended
  full-size `Sharp`, integer `Pixel exact`, and fractional nearest-neighbour
  `Classic`.
- Limit the sharp filter's transitions to approximately one physical display
  pixel, avoiding both blurry bilinear output and the uneven block sizes of
  fractional nearest-neighbour scaling.
- Render the palette-based software cursor in the same Metal pass, then compose
  touch and Pencil feedback through SDL.
- Preserve the original 16:10 aspect ratio in every mode and retain the former
  SDL renderer as an automatic fallback if Metal shader creation fails.

Later improvement:

- Use spare letterbox regions for optional controls rather than stretching the
  original artwork.

## P2: Optional visual modernization

The modern presentation is an optional skin layered over the original game.
Simulation, 640-by-400 logical coordinates, hit testing, pathfinding and save
games must remain identical, and every replacement must fall back to the
original artwork independently.

### Stage 1: Modern image filter

Implemented and checked on the physical iPad; the deliberately restrained
whole-frame effect is most visible on diagonal sprite edges and does not by
itself add source detail:

- Add an independent `Classic / Modern` artwork-style switch without changing
  the three existing presentation/scaling modes.
- Apply edge-aware corner reconstruction, restrained local detail enhancement
  and palette-safe color shaping in the indexed Metal shader.
- Preserve black levels, palette fades, player colors, movies and the exact
  classic output when the feature is disabled.

### Stage 2: HD asset-pack foundation

Foundation and first visible proof asset implemented:

- Define a versioned manifest and stable asset identifiers for optional packs
  stored in the Files-visible user directory.
- Validate identifiers, scale, duplicate entries and safe relative PNG paths;
  reject malformed or path-traversing packs without affecting startup.
- Discover packs at startup, report their status in diagnostics and guarantee
  per-asset fallback when a replacement is absent.
- A host test covers valid manifests, duplicate IDs, version rejection and path
  traversal. The physical startup confirms absent-pack fallback.
- Bundle an original 4x HD default cursor as the first proof asset and load it
  directly as an RGBA Metal texture. It replaces only the normal pointer in
  Modern mode; every action-specific cursor keeps its original semantics.
- Composite selected units, infantry and buildings with a resolution-independent
  Metal frame. It follows the engine's exact adjusted object bounds, uses each
  owner's bright house color and supports multiple selections. Modern mode now
  suppresses the classic indexed corners; Original mode and Metal failure retain
  the classic path.
- Track selection overlays by object so the legacy dirty-rectangle renderer
  does not make them disappear on otherwise unchanged frames. Movement and
  deselection update or remove entries without forcing continuous redraws.
- Prefer a valid Files-visible user pack, otherwise load the built-in proof
  pack. A missing, malformed or undecodable asset falls back without blocking
  the game. Physical diagnostics confirm both manifest and 114-by-128 texture
  loading.

The selection indicator is physically accepted. The first battlefield proof
asset is also implemented:

- Replace the Nod buggy in Modern mode with an original 4x RGBA atlas containing
  32 chassis and 32 independently rotating turret facings.
- Preserve the engine's 24-by-24 logical footprint, body/turret offsets, movement,
  aiming and owner bright colour. Neutral paint panels are tinted in Metal from
  the live palette.
- Suppress the indexed body and turret while the HD Buggy is active, recreate
  its grounding shadow in Metal and draw its health bar as a top overlay.
  Cloaked/special states, missing/invalid atlases, Original mode and Metal
  failure retain the automatic original-sprite fallback.
- Generate the atlas reproducibly from separate source components with
  `tools/artwork/build_buggy_atlas.py`. The physical iPad confirms the 768-by-768,
  64-frame atlas and shader pipeline load successfully.
- Correct the Buggy turret around its actual circular mounting pivot rather
  than the asymmetric alpha bounds.
- Add the GDI Humvee through the same 64-frame body/turret replacement path,
  with its own mounting-ring pivot and automatic original fallback.
- Add the shared GDI/Nod E1 Minigunner as an 80-frame atlas covering ten common
  poses in eight directions. House color is applied live; rare special/death
  actions remain on the frame-accurate original sprites.

Next:

- Physically accept buggy scale, direction, alignment, turret tracking and house
  colour, then use the same draw-command model for one representative building.

### Stage 3: HD battlefield renderer

First representative vehicle path implemented; broader capture remains planned:

- Capture terrain, building, vehicle, infantry, shadow and animation draw
  commands before they are flattened into the indexed framebuffer.
- Composite high-resolution texture atlases in Metal while retaining original
  coordinates, z-order, selection logic and damage frames.
- Reproduce house-color remapping, translucency, cloaking, fog, shroud and
  palette animation, with automatic original fallback for every draw command.
- Establish texture-memory, loading-time and sustained-performance budgets on
  physical iPads.

### Stage 4: Modern menus and native overlays

Planned:

- Add high-resolution fonts, buttons, backgrounds and selection indicators.
- Move appropriate auxiliary controls into optional native iPad overlays and
  unused letterbox space without covering the tactical viewport.
- Keep the complete classic menu path available instantly and preserve German
  and English localization plus accessible text alternatives.

Distribution note: source-code availability does not grant redistribution
rights for the original commercial artwork. Public packs should contain
originally created art, or a local conversion process should derive private
replacements from game data supplied by the user.

## P2: Accessibility and platform polish

Planned work:

- Add UI scaling and enlarged interaction targets.
- Add higher-contrast selection and cursor options.
- Investigate color-vision-friendly team and status indicators.
- Add subtitles or transcript support for important spoken and movie content
  where source material permits it.
- Expose native overlay controls to VoiceOver, Switch Control, and Voice
  Control.
- Use controller or Apple Pencil Pro haptics sparingly for confirmed actions.
- Add a proper AppIcon asset catalog and a designed launch screen.
- Test current App Store packaging requirements separately from personal device
  builds, including licensing and game-data distribution constraints.

## Feature decisions after the core iPad work

### Multiplayer

The iPad preset currently builds with networking disabled while the legacy menu
can still expose multiplayer options. Decide between:

- hiding unavailable network choices; or
- restoring and modernizing LAN/Internet play, including local-network privacy
  declarations, discovery, background behavior, and compatibility testing.

Game Center integration is a separate, higher-effort project.

### Red Alert

Keep the first milestones focused on Tiberian Dawn. Once windowing, lifecycle,
input, storage, audio, and packaging are stable, move the shared iPad platform
layer to VanillaRA and validate the Red Alert data importer separately.

## Verification matrix

Before calling the iPad port production-ready, test:

- physical arm64 iPad Debug and Release builds;
- iPadOS 15 deployment compatibility and current iPadOS behavior;
- landscape, portrait, Stage Manager, external displays, and window resizing;
- touch-only, Pencil, mouse, trackpad, hardware keyboard, and controller input;
- normal and Low Power Mode frame pacing;
- silent mode, Bluetooth audio, interruptions, and route changes;
- suspend/resume, memory warnings, termination, autosave recovery, and storage
  exhaustion;
- GDI and Nod campaigns, cutscenes, save/load, skirmish, and long sessions;
- import failures, incomplete discs, corrupt archives, and low-storage cases.

The adaptive layout, Metal renderer, startup, campaigns, lifecycle recovery,
performance telemetry, primary touch path, and Stage Manager resizing have
been validated on the physical iPad. Stage Manager testing covered repeated
live resizing from fullscreen down to roughly 375-by-486 points and back,
compact-window guidance, touch input, aspect preservation, idle/background
throttling, and stable memory/thermal behavior. A persistent SDL draw-color
leak discovered during this pass was fixed so touch-feedback colors can no
longer tint newly exposed letterbox margins. External-display validation is
explicitly deferred to a later, non-blocking pass.

Automated fault hardening now covers atomic recovery writes, staging
replacement and rollback, storage-capacity boundaries, ISO-9660 bounds and
InstallShield V3 corruption. Save-transfer policy adds a nineteenth host test.
Language resolution and catalog fallback add a twentieth host test. Native
audio queue behavior adds a twenty-first. HD-pack manifest and path hardening
adds a twenty-second. All 22 host tests pass, and signed physical-iPad
Debug and Release builds succeed. The real GDI and Nod Gold ISOs also pass the
stricter parser and representative `SETUP.Z` extraction.
