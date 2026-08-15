# Apple platform feature parity

The shipping iPadOS and macOS apps and the native visionOS command-window
prototype use the same game engine and shared Apple enhancement layer. New
features are expected to reach all three build targets unless an operating-
system capability makes that impossible. `Designed for iPad` compatibility is
a separately tested bridge and never substitutes for native-target validation.

Shared today:

- adaptive 16:10 presentation with sharp, pixel-perfect, and classic modes;
- Metal palette rendering and the optional modern-art proof pack;
- modern cursor, Buggy, Humvee, MCV, standard infantry and faction-specific
  power-plant, infantry-production and Construction-Yard replacements;
- large-cursor/high-contrast mode, UI scaling, controller support, and
  German/English menus;
- AVAudioEngine playback, local Gold-CD import, save-file access, and the
  shared local/private-room multiplayer protocol and classic match lobby;
- SDL text input for ordinary edit fields and the post-mission high-score
  entry; UIKit presents the iPadOS keyboard or visionOS spatial keyboard.

Platform-specific integrations remain native: touch, Pencil, gaze/pinch, safe areas,
Stage Manager, iPad interruption recovery, Files pickers, AppKit import panels,
and Finder/iCloud save-folder access.

The visionOS command window additionally maps system look/pinch delivery onto
the shared logical cursor, supports system-managed Look to Scroll on visionOS
26, and keeps a selection rectangle active while the tactical map scrolls at
an edge. Issued move and attack commands use the same green/red semantic
feedback as direct touch. Direct iPadOS and visionOS pans use a single vector
and a clean tactical redraw to avoid retaining multi-cell sprite fragments.
Its speed, edge band, and selection tolerance are user-configurable;
these presentation/input aids do not change engine hit testing or simulation.

Every pull request touching shared behavior must pass all three Apple compile
jobs. A shared feature remains incomplete until its behavior is
implemented or explicitly documented as unavailable on all three platforms.

Multiplayer is implemented as one shared deterministic protocol and lobby model
with native transports and lifecycle handling. It targets cross-play rather
than three platform-specific multiplayer forks. See the
[multiplayer architecture](MULTIPLAYER-CONCEPT.md).

The native Vision Pro design keeps the original game surface and engine shared,
then adds optional visionOS window, volume, immersion, input, and audio layers.
See the [Vision Pro concept](VISIONOS-CONCEPT.md).
