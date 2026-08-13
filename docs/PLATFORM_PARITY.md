# Apple platform feature parity

The shipping iPadOS and macOS apps use the same game engine and shared Apple
enhancement layer. New features are expected to ship on both platforms unless
an operating-system capability makes that impossible. A planned native
visionOS target joins this parity contract when its command-window milestone
is enabled; until then, `Designed for iPad` compatibility is a separately
tested bridge rather than a claim of native parity.

Shared today:

- adaptive 16:10 presentation with sharp, pixel-perfect, and classic modes;
- Metal palette rendering and the optional modern-art proof pack;
- modern cursor, Buggy, Humvee, and standard infantry replacements;
- large-cursor/high-contrast mode, UI scaling, controller support, and
  German/English menus;
- AVAudioEngine playback, local Gold-CD import, save-file access, and the
  shared local/private-room multiplayer protocol and classic match lobby.

Platform-specific integrations remain native: touch, Pencil, safe areas,
Stage Manager, iPad interruption recovery, Files pickers, AppKit import panels,
and Finder/iCloud save-folder access.

Every pull request touching shared behavior must pass both Apple compile jobs.
Once the native visionOS target enters the repository, its compile job becomes
mandatory too. A shared feature then remains incomplete until its behavior is
implemented or explicitly documented as unavailable on all three platforms.

Multiplayer is implemented as one shared deterministic protocol and lobby model
with native transports and lifecycle handling. It targets cross-play rather
than three platform-specific multiplayer forks. See the
[multiplayer architecture](MULTIPLAYER-CONCEPT.md).

The native Vision Pro design keeps the original game surface and engine shared,
then adds optional visionOS window, volume, immersion, input, and audio layers.
See the [Vision Pro concept](VISIONOS-CONCEPT.md).
