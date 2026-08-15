# Changelog

All notable changes to Tiberian Dawn for Apple platforms are documented here.
The project keeps iPadOS, macOS, and visionOS on one shared engine and content
baseline wherever the operating systems permit it.

## 0.2.3 — 2026-08-15

This release publishes the current shared Apple-platform source together with
a signed, notarized Universal 2 macOS package for Apple Silicon and Intel.

### Added

- Native visionOS command-window target, build guide, simulator/device
  workflows, and physical Vision Pro validation.
- Vision Pro gaze and pinch input, selection-box gestures, edge and look-to-
  scroll map movement, and mission-safe gesture-state handling.
- Touch-first iPad map scrolling, accurate drag selection, contextual move and
  attack feedback, Apple Pencil input, and software-keyboard support.
- Touch, Pencil, mouse, and trackpad shortcuts for skipping videos and
  cutscenes.
- Expanded optional modern-art mode with GDI and Nod infantry, Buggy/Humvee,
  mobile construction vehicles, Power Plants, Barracks/Hand of Nod, and
  Construction Yards.
- Deterministic artwork-atlas builders, source references, validation, and the
  modern-art asset contract for future contributors.

### Changed

- Shared rendering, settings, localization, import, save, audio, and
  multiplayer behavior now explicitly covers iPadOS, macOS, and visionOS.
- Modern unit and structure overlays preserve classic gameplay state while
  matching movement direction, health display, construction activity, map
  clipping, and faction presentation.
- iPad input differentiates taps, selection drags, one- and two-finger map
  movement, Pencil, pointer accessories, and text-entry focus more reliably.
- Vision Pro input is reset across mission, dialog, and lobby transitions so
  the same controls remain available throughout campaigns and multiplayer.

### Fixed

- Right-edge gaze scrolling on Vision Pro now uses the complete command-window
  width.
- visionOS builds made with pre-26 SDKs now compile without the optional
  Look-to-Scroll API and retain the custom edge-scroll fallback.
- Modern artwork no longer reveals enemies through fog of war, leaks over the
  sidebar or menus, or remains visible after leaving a mission.
- Infantry and vehicle modern artwork follows the commanded travel direction.
- Touch scrolling no longer leaves stale building fragments or a frozen
  selection cursor on the map.
- Selection rectangles align with the user's touch point and remain usable
  after attaching or removing a keyboard accessory.

### Known limitations

- Multiplayer remains a beta and needs further long-session, adverse-network,
  suspend/recovery, and maximum-player testing.
- Native visionOS is a physical-device-tested prototype. Source and build
  instructions are public, but there is no public visionOS binary yet.
- Modern artwork is optional and currently covers an initial set of units and
  structures; the original graphics remain the complete fallback.

## 0.2.2 — 2026-08-13

- Aligned public version metadata for iPadOS and macOS at 0.2.2 (202).
- Centralized Apple bundle version metadata and added a CI parity check.
- Retained the signed, notarized Universal 2 macOS package and all gameplay,
  multiplayer, window, rendering, import, save, and audio improvements from
  0.2.1.

No release contains original Command & Conquer game data. Players must supply
their own lawfully usable C&C Gold GDI and Nod discs or disc images.
