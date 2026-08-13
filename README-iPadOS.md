# Tiberian Dawn for iPad — iPadOS port

This is a modified, work-in-progress iPadOS port based on Vanilla Conquer. It is
not an Electronic Arts product and is not affiliated with or endorsed by
Electronic Arts. The repository remains subject to `License.txt` and the
additional terms it contains.

No game data is included. You must supply data from a copy you are legally
entitled to use.

## Requirements

- Apple Silicon Mac
- Xcode with its license accepted and iOS platform components installed
- CMake 3.25 or newer
- An Apple ID added to Xcode for personal-device signing

Do not create an empty project in Xcode.

## Recommended safe setup

Download the repository source archive or clone it, then double-click this file
in the project folder:

```text
Open Tiberian Dawn for iPad.command
```

If macOS shows its normal first-open confirmation for the downloaded command
file, Control-click it, choose Open, and confirm once.

The starter checks the local tools, retrieves the exact SDL2 revision recorded
by the project if necessary, verifies and applies the checked-in iPad patch,
generates `TiberianDawnApple.xcodeproj`, and opens it. It never uses `sudo`,
installs packages, changes Apple credentials, or reads game data.

In Xcode, select the `TiberianDawn` scheme. Open the target's Signing &
Capabilities tab, choose your Apple Development Team, and use a unique Bundle
Identifier if Xcode requests one. Select the connected iPad and press Run.

The remaining commands in this document are for contributors and repeat-build
automation. Normal users do not need to create a CMake signing preset.

## Contributor workflow for the canonical physical-iPad project

The physical iPad build is the canonical development and acceptance target.
All new work must be built and verified with `ipados-device` on real hardware.
The simulator preset remains in the repository only as an unused legacy
fallback and is not part of the normal workflow.

Copy the private user-preset template once:

```sh
cp resources/CMakeUserPresets.json.example CMakeUserPresets.json
```

Edit the ignored `CMakeUserPresets.json`, replacing the example bundle ID and
team ID. Then prepare SDL and configure:

```sh
./scripts/prepare-ipados-dependencies.sh
cmake --preset ipados-device-local
```

Then open:

```text
build/ipados-device/TiberianDawnApple.xcodeproj
```

Select the `TiberianDawn` scheme and the connected iPad. Signing is managed
automatically using the values in your ignored local preset.

The generated physical-device Xcode project and all app products deliberately
live below `/private/tmp/TiberianDawn-iPad-Device-*`. This avoids Xcode-generator
quoting issues and prevents iCloud Drive/File Provider metadata on a project
stored in Documents from invalidating the code signature. The source remains
in this repository.

Build the signed physical-device product with:

```sh
cmake --build --preset ipados-device-local
```

## Data location and first-launch import

On first launch the app asks for both original C&C Gold ISOs (GDI and Nod), or
for a previously prepared data directory. The import is validated and committed
atomically. Immutable assets are stored privately in:

```text
Library/Application Support/TiberianDawn/vanillatd/
```

Settings, manual saves, and rotating recovery saves remain visible through
Files below `Documents/TiberianDawn/vanillatd`. Remastered Collection, Ultimate
Collection, and First Decade data are not supported by this importer.

The first-launch guide names the expected ISO files, illustrates the joint
selection workflow, and covers Files, iCloud Drive, USB storage, the 1.2 GB
free-space requirement and local-only processing. Import runs off the UI thread
behind a visible progress screen. Errors offer actionable checks and retry;
`Später einrichten` exits cleanly and shows the guide again next time.

## iPad performance modes

Normal play targets 60 FPS. Open the in-game options, choose `Game Controls`,
and tap `Battery mode: OFF` to switch to the persistent 30 FPS battery mode.
The label changes to `Battery mode: ON`. iPadOS Low Power Mode and serious or
critical thermal pressure also impose the 30 FPS cap automatically.

The iPad path renders the original 640 by 400 indexed framebuffer directly
with Metal. Changed frames upload only the changed rectangle of an 8-bit index
texture (at most one quarter of the old 32-bit upload), while palette changes
and fades update only a 256-color palette texture. Static menus and paused
scenes avoid uploads and presentation entirely. Rendering is suspended while
the app is inactive or backgrounded and a fresh frame is forced when the app
returns. If the custom pipeline cannot be created, the previous SDL conversion
path remains available automatically.

The Debug device build prints an `iPad perf:` line every five seconds in the
Xcode console. It includes target/actual FPS, upload rate, skipped frames,
average present time, CPU, resident memory, touch latency, Low Power Mode, and
thermal state. Use these lines together with Instruments Energy Log for long
mission, movie, screen-recording, and suspend/resume tests.

## Optional modern artwork

Visual Controls has an independent `Style: Original / Modern` switch. Modern
uses the indexed Metal source directly for edge-aware corner reconstruction,
restrained local detail and palette-safe color shaping. It does not change
game coordinates, hit testing, simulation, saves, or the selected `Sharp / Pixel
exact / Classic` presentation mode. Original bypasses all modernization and
preserves the previous output exactly.

The HD-pack system first looks for an optional user pack below:

```text
Documents/TiberianDawn/vanillatd/ModernArt/manifest.ini
```

The versioned INI manifest declares a pack ID, display name, 2x or 4x scale,
and stable asset-ID-to-PNG mappings. Startup rejects duplicate IDs, unsafe or
absolute paths, traversal attempts, unsupported versions, missing files and
invalid PNG signatures, then continues safely. Assets omitted from a valid
manifest also fall back individually. See
`resources/apple/ModernArt-manifest.example.ini` for the format.

When no user pack is present, the app loads its small built-in proof pack. In
Modern mode it composites a genuinely higher-resolution 4x normal cursor as a
separate RGBA Metal texture; action-specific move, attack, repair and targeting
cursors still use their originals so their meaning cannot be lost. Original
mode bypasses this replacement. Failure to load either the manifest or PNG
activates the original cursor automatically.

Modern mode also adds a native-resolution selection frame around units,
infantry and buildings. Its dark outer contour and inner line follow the
selected object's house color.
Bounds come from the same engine dimensions used for hit feedback and health
bars, including infantry and barracks offsets. Multiple selections are tracked
without continuous framebuffer redraws. Original mode retains the indexed white
corners; Modern mode suppresses them.

The bundled proof pack now also replaces the Nod buggy in Modern mode. Its
768-by-768 RGBA atlas has 32 chassis and 32 turret facings, so the machine gun
continues to aim independently of movement. Metal composites the 4x artwork at
the original 24-by-24 logical footprint and tints neutral paint panels from the
owner's live palette colour. While the HD path is active, the indexed body and
turret are suppressed; Metal recreates a soft grounding shadow and draws the
health bar above the replacement. Cloaked/special visual states and a missing
atlas or Metal path automatically use the original sprite. The source master
and deterministic atlas builder live below
`resources/apple/ModernArt/units/buggy/source` and
`tools/artwork/build_buggy_atlas.py`.

Modern mode also replaces the GDI Humvee using the same independently rotating
vehicle path. Both vehicle atlases now define the circular weapon-mount pivot
explicitly, preventing long asymmetric gun barrels from moving the turret off
center while it turns. The shared GDI/Nod E1 Minigunner uses an 80-frame atlas:
ten standing, walking, firing, prone, crawling, and transition poses across all
eight facings. Runtime house-color tinting differentiates the factions. Rare
special/death actions automatically retain their frame-accurate original art.

## Physical-device status

The Xcode 26.6 device build succeeds and runs on the physical iPad with the
complete GDI and Nod C&C Gold data. Absolute iOS container paths preserve their
case-sensitive system components, both disc directories are detected, and the
game reaches live missions. The app uses UIKit scenes, supports all four iPad
orientations, and adapts live to drawable-size and safe-area changes. The
classic 640 by 400 image stays centered at 16:10, while touch and mouse
coordinates are mapped through the same layout transform.

When an active single-player mission enters the background, the engine pauses
simulation and audio, flushes settings, and writes a transactional recovery
save on the game thread. Two rotating slots protect the previous committed
state. If iPadOS later terminates the app, the next launch offers Continue;
finishing the mission or choosing Main Menu removes these private recovery
slots. Normal manual save games are unaffected.

Physical-device startup, GDI mission loading, primary touch selection, direct
main-menu exit, signing, persistent imported data, rotating recovery saves and
restoration after an explicit SIGKILL have been verified. Returning to the main
menu removes only the recovery slots and preserves normal manual saves.
External-display validation is intentionally deferred to a later pass and is
not part of the current acceptance scope.

Host-side hardening tests now exercise the same production layout calculation
across fullscreen, portrait, compact Stage Manager, Retina safe-area,
pixel-perfect fallback and ultrawide external-display geometries. A lifecycle
test also guarantees that duplicate notifications collapse without losing a
rapid background/foreground cycle before the next engine tick.

The same host suite now injects partial/failed recovery writers, empty saves,
staging replacement and rollback failures, insufficient or unknown storage,
malformed ISO-9660 records and corrupt InstallShield V3 archives. A nineteenth
test covers manual-save identification, free-slot allocation and safe export
names. A twentieth test covers system-language resolution, explicit German and
English preferences, and the English fallback. A twenty-first test covers the
native audio stream queue across saturation, draining, pause and reset. A
twenty-second validates HD-pack manifests, duplicate IDs and path traversal.
All 22 tests pass. Both real Gold ISOs pass the stricter production parser and representative
`SETUP.Z` extraction; signed Debug and Release device builds also succeed.

The iPad main menu and in-mission Game Controls now expose
`Spielstände / Dateien`. The native manager imports one or several compatible
saves from Files/iCloud/USB into free internal slots and exports a selected save
or all valid manual saves as registered `.cncsave` documents. Existing saves,
private recovery slots and settings are never overwritten or exported.

The iPad additions are bilingual in German and English. The default follows the
iPad system language (all other languages fall back to English), while Game
Controls offers a persistent `System / Deutsch / English` choice. Native import,
save-transfer, compact-window and error UI updates immediately. The classic
engine text table is selected on the next app start: `CONQUER.GER` is preferred
for German when supplied by the original data, with `CONQUER.ENG` as the safe
fallback.

Physical Stage Manager resizing is verified from fullscreen down to roughly
375-by-486 points and back. The viewport remains contained, centered and 16:10;
touch remains aligned; compact guidance appears; and newly exposed letterbox
margins are explicitly cleared to black after touch or Pencil feedback.

The isolated fresh-install workflow has also been verified on the physical
iPad: the guide reappears after `Später`, a single GDI ISO produces the expected
recoverable error, both Gold ISOs import successfully, and a subsequent launch
skips the importer and starts the game directly. iPadOS represents `.iso` files
with an extension-derived dynamic type rather than `public.disk-image`; the
picker explicitly accepts that precise type so ISO files remain selectable
without enabling arbitrary documents.

## Interruption recovery files

Recovery files live beside the current settings and save files during this
bring-up phase:

```text
autosave.ipad0
autosave.ipad1
autosave.ipad.tmp
```

The `.TMP` file exists only while a new save is being serialized. It is
synchronized and atomically renamed before it can become a recovery candidate.
Do not copy these files between installations as manual saves; use the regular
in-game save slots for that purpose.

## Current controls

- One-finger tap: primary click/select or context command
- One-finger drag: selection drag after a small movement threshold
- Two-finger drag: pan the tactical map
- Long press: secondary click/cancel/deselect
- Two-finger tap: alternative secondary click
- Apple Pencil: precise primary selection
- Apple Pencil hover: blue target preview on supported iPads
- Apple Pencil double-tap: secondary click/cancel; Pencil Pro squeeze: back
- Bluetooth/USB mouse and trackpad: absolute hover, left/right/middle click,
  drag, and wheel scrolling through the adaptive viewport transform
- On-screen keyboard: opens automatically for editable text fields
- Hardware keyboard: active system layout for text, physical keys for commands
- Controller: automatically detected; left stick moves the pointer, right stick
  pans, A/Cross selects, B/Circle cancels, X/Square guards, Y/Triangle forms
- `Game Controls > Controller layout`: labels matched to Xbox, PlayStation, or
  Nintendo controllers

Finger taps show a yellow ring, Pencil input a smaller blue ring, and classic
controls receive enlarged invisible finger hit regions. Interrupted or
cancelled gestures release any held selection button. Pinch adjusts the touch
UI between 100 and 150 percent. Visual Controls offers three GPU presentation
modes: `Sharp` is the default full-size mode with display-pixel-sized edge
transitions; `Pixel exact` uses integer scaling; and `Classic` preserves
fractional nearest-neighbour scaling. The same screen also offers a
large-cursor/high-contrast mode plus the independent `Original / Modern`
artwork style. True tactical zoom still needs a separate engine change because
the original viewport assumes a fixed scale.
