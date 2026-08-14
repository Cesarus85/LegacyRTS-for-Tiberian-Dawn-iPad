# Apple platform feature parity

The shipping iPadOS and macOS apps and the native visionOS command-window
prototype use the same game engine and shared Apple enhancement layer. New
features are expected to reach all three build targets unless an operating-
system capability makes that impossible. `Designed for iPad` compatibility is
a separately tested bridge and never substitutes for native-target validation.

Shared today:

- adaptive 16:10 presentation with sharp, pixel-perfect, and classic modes;
- Metal palette rendering and the optional modern-art proof pack;
- modern cursor, Buggy, Humvee, and standard infantry replacements;
- large-cursor/high-contrast mode, UI scaling, controller support, and
  German/English menus;
- AVAudioEngine playback, local Gold-CD import, save-file access, and the
  shared local/private-room multiplayer protocol and classic match lobby.

Platform-specific integrations remain native: touch, Pencil, gaze/pinch, safe areas,
Stage Manager, iPad interruption recovery, Files pickers, AppKit import panels,
and Finder/iCloud save-folder access.

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
