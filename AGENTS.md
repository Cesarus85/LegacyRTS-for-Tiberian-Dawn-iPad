# Apple platform parity

The iPadOS and macOS apps are two native targets of one product. Keep them at
the same functional level whenever the operating systems permit it.

- Implement engine, graphics, audio, localization, save-game, accessibility,
  menu, and content features in shared code by default.
- A new shared feature is incomplete until it is enabled and tested for both
  `IPADOS_PORT` and `MACOS_PORT`.
- Restrict platform branches to capabilities that truly require UIKit/AppKit
  or device hardware. Examples are direct touch and Pencil input, iPad safe
  areas and Stage Manager, iPad audio-session interruptions, and macOS Finder
  integration.
- When behavior must differ, preserve equivalent user-facing capability and
  document the reason next to the conditional.
- Never publish or merge a release-facing change after testing only one Apple
  target. Run the host tests, unsigned iPadOS compile, and Universal 2 macOS
  package verification.
- Use `ipados-simulator` for fast day-to-day iteration when a physical iPad is
  unavailable. Keep its generated project separate from `ipados-device`, and
  still validate touch, Pencil, performance, audio interruptions, lifecycle,
  and release candidates on a physical iPad before declaring work complete.
- Keep shared Apple resources below `resources/apple`; do not create divergent
  iPad and Mac copies of the same artwork.
- Treat multiplayer as one protocol and simulation feature across Apple
  targets. Platform code may implement discovery, permissions, and lifecycle,
  but must not create incompatible iPad, Mac, or visionOS game rules or wire
  formats. Follow `docs/MULTIPLAYER-CONCEPT.md` and keep release presets offline
  until its acceptance gate is met.
- Treat `Designed for iPad` on Apple Vision Pro as a compatibility target, not
  proof of a native port. When `VISIONOS_PORT` is introduced, shared features
  must include it by default and the native compile/device gates in
  `docs/VISIONOS-CONCEPT.md` become part of release acceptance.
