# iPadOS and macOS feature parity

Both apps use the same game engine and shared Apple enhancement layer. New
features are expected to ship on both platforms unless an operating-system
capability makes that impossible.

Shared today:

- adaptive 16:10 presentation with sharp, pixel-perfect, and classic modes;
- Metal palette rendering and the optional modern-art proof pack;
- modern cursor, Buggy, Humvee, and standard infantry replacements;
- large-cursor/high-contrast mode, UI scaling, controller support, and
  German/English menus;
- AVAudioEngine playback, local Gold-CD import, save-file access, and the
  disabled multiplayer placeholder.

Platform-specific integrations remain native: touch, Pencil, safe areas,
Stage Manager, iPad interruption recovery, Files pickers, AppKit import panels,
and Finder/iCloud save-folder access.

Every pull request touching shared behavior must pass both Apple compile jobs.
