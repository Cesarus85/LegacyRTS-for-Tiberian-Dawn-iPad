# Legacy RTS — iPadOS port

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
- Ninja (used for the fast macOS baseline build)

Do not create an empty project in Xcode. CMake owns the project structure.

## Canonical physical-iPad project

The physical iPad build is the canonical development and acceptance target.
All new work must be built and verified with `ipados-device` on real hardware.
The simulator preset remains in the repository only as an unused legacy
fallback and is not part of the normal workflow.

Copy the private user-preset template once:

```sh
cp resources/CMakeUserPresets.json.example CMakeUserPresets.json
```

Edit `CMakeUserPresets.json` and replace `com.example.legacy-rts` and
`YOUR_TEAM_ID`. This ignored file keeps personal signing information out of the
public repository. Then prepare the SDL dependency and configure:

```sh
./scripts/prepare-ipados-dependencies.sh
cmake --preset ipados-device-local
```

Then open:

```text
build/ipados-device/VanillaConquer.xcodeproj
```

Select the `VanillaTD` scheme and `iPad von Stefan`. Signing is managed
automatically with the development team stored in the CMake cache.

The generated Xcode project deliberately places simulator and device app
products under `/private/tmp/LegacyRTS-iPad-*-Products`. This prevents iCloud
Drive/File Provider metadata on a project stored in Documents from invalidating
Xcode's code signature. The source and Xcode project remain in this repository.

Build the signed physical-device product with:

```sh
cmake --build --preset ipados-device-local
```

## Data location and first-launch import

On first launch the app asks for both original C&C Gold ISOs (GDI and Nod), or
for a previously prepared data directory. The import is validated and committed
atomically. Immutable assets are stored privately in:

```text
Library/Application Support/LegacyRTS/vanillatd/
```

Settings, manual saves, and rotating recovery saves remain visible through
Files below `Documents/LegacyRTS/vanillatd`. Remastered Collection, Ultimate
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

Static frames are dirty-rendered: unchanged menus and paused scenes avoid the
full 640 by 400 conversion, texture upload, and present. Rendering is suspended
while the app is inactive or backgrounded and a fresh frame is forced when the
app returns.

The Debug device build prints an `iPad perf:` line every five seconds in the
Xcode console. It includes target/actual FPS, upload rate, skipped frames,
average present time, CPU, resident memory, touch latency, Low Power Mode, and
thermal state. Use these lines together with Instruments Energy Log for long
mission, movie, screen-recording, and suspend/resume tests.

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
main-menu exit, signing, and persistent imported data have been verified.
Stage Manager, external displays, and repeated interruption recovery still
need broader on-device testing.

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
UI between 100 and 150 percent. Visual Controls also offers full-size or
pixel-perfect presentation and a large-cursor/high-contrast mode. True tactical zoom needs a
separate engine change because the original viewport assumes a fixed scale.
