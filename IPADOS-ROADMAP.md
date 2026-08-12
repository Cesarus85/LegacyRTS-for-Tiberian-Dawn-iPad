# Tiberian Dawn for iPad — optimization roadmap

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
  `Library/Application Support/TiberianDawnForiPad/vanillatd` and are excluded from
  backup. Settings, manual saves, and recovery saves remain in the
  Files-visible `Documents/TiberianDawnForiPad/vanillatd` directory.
- `AVAudioSession` uses the system-silent-switch policy and observes audio
  interruptions, route/Bluetooth changes, and media-service resets.
- Visual Controls offers full-size sharp or integer pixel-perfect rendering,
  100/125/150 percent touch UI, and a large-cursor/high-contrast mode. Pinch
  also adjusts the touch UI scale directly.
- Compact Stage Manager windows show an accessible minimum-size warning.
- A custom original AppIcon, launch color, privacy manifest, and signed
  `ipados-device-release` preset are part of the Xcode target.
- Multiplayer is visibly unavailable and disabled while networking is omitted.
  Red Alert reuse is prepared through the shared `platform/apple` layer, but
  enabling and validating the separate game target remains its own milestone.

The legacy OpenAL decoder backend, a custom indexed-palette Metal shader, true
tactical zoom, network transport, and full framebuffer semantics for VoiceOver
remain separate engine-scale projects rather than release blockers for the
single-player iPad port.

## Current status

- TiberianDawn builds, signs, installs, and launches on the physical iPad.
- SDL uses the accelerated Metal renderer with VSync.
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
- Game assets are stored below `Library/Application Support/TiberianDawnForiPad/vanillatd`;
  settings and saves remain below `Documents/TiberianDawnForiPad/vanillatd`.
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

Remaining work:

- Define sensible minimum window sizes and pause or show guidance when a window
  is too narrow for usable gameplay.
- Evaluate SDL 3.4+ or `sdl2-compat` because the vendored SDL 2.32.10 backend
  still uses the legacy UIKit application lifecycle.
- Complete the Stage Manager and external-display device test matrix.

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
- Incomplete temporary saves are removed and never offered for recovery.
- A valid newest slot produces an Interrupted Mission/Continue prompt after
  relaunch. Recovery slots are removed after the mission ends or the player
  declines recovery.
- A memory warning releases reconstructible interpolated-palette caches when no
  movie is active.

Remaining work:

- Validate repeated suspend/resume, forced termination, low-storage failure,
  and recovery on a physical iPad.
- Add explicit inactive-state policy and modern audio-session interruption and
  route-change handling; these remain part of the audio milestone.
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

The present manual Files workflow is suitable for bring-up, but immutable game
assets, saves, and settings should not permanently share one visible Documents
directory.

Planned work:

- Add a first-launch import screen using the system document picker.
- Let the user choose the GDI and Nod C&C Gold ISOs or an already prepared data
  directory.
- Verify the expected disc structure and required MIX files.
- Extract the required files, including `INSTALL/SETUP.Z`, with progress and
  clear error reporting.
- Check available storage before importing and avoid retaining temporary ISO
  copies unnecessarily.
- Store immutable game assets under `Library/Application Support` and exclude
  reconstructible assets from backup.
- Keep settings and save games separate from original game assets.
- Provide explicit save-game import and export through Files or iCloud Drive.
- Never bundle proprietary original game data with the source port.

## P2: Audio modernization

The iPad target currently links Apple's OpenAL framework. The current SDK marks
OpenAL deprecated in favor of AVAudioEngine.

Planned work:

- Configure an appropriate `AVAudioSession` category for a game.
- Respect the system mute policy selected for the app.
- Handle Siri, alarms, audio interruptions, route changes, Bluetooth devices,
  and media-service resets.
- Resume music, speech, sound effects, and movie audio without overlapping or
  losing position.
- Replace the Apple OpenAL backend with SDL Audio or AVAudioEngine after the
  lifecycle behavior is covered by tests.

## P2: Rendering and presentation quality

The existing SDL Metal renderer is already accelerated and is adequate for the
first production-quality milestones. A complete renderer rewrite is not the
first priority.

Later improvements:

- Offer a pixel-perfect integer-scale mode and a full-size sharp presentation
  mode.
- Avoid uneven nearest-neighbor pixel sizes on Retina displays.
- Implement an indexed 8-bit Metal texture plus a 256-color palette texture so
  palette conversion and fades occur on the GPU.
- Upload only the smaller index texture or dirty regions when practical.
- Use spare letterbox regions for optional controls rather than stretching the
  original artwork.
- Preserve the original aspect ratio by default.

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

Multiplayer is work in progress and planned for a later development stage. The
iPad preset currently builds with networking disabled while the existing menu
can still expose multiplayer options. Later work includes:

- hiding unavailable network choices until the implementation is ready; and
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
performance telemetry, and primary touch path have been validated on the
physical iPad. Stage Manager and external-display validation remain
outstanding.
