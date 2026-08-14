# Tiberian Dawn for Apple Vision Pro

![Original concept artwork for the native Vision Pro prototype](docs/assets/vision-pro-tiberian-dawn-collage-v1.png)

The repository contains two deliberately separate Vision Pro paths:

1. the existing iPad app running in Apple's **Designed for iPad** compatibility
   environment; and
2. an arm64 **native visionOS command-window prototype** built from the same
   engine, Metal renderer, AVAudioEngine backend, importer, saves, localization,
   controls, modern-art resources, and networking code.

Neither path includes original Command & Conquer game data. Supply legally
owned C&C Gold GDI and Nod disc images through the system importer.

## Native simulator build

Requirements: Xcode with the visionOS SDK and CMake 3.28 or newer.

```sh
./scripts/build-visionos-simulator.sh
```

The generated Xcode project is
`build/visionos-simulator/TiberianDawnApple.xcodeproj`; the Debug application is
written to
`/private/tmp/TiberianDawnApple-Vision-Simulator-Products/Debug/TiberianDawn.app`.

For an unsigned device compile without installing on a headset:

```sh
./scripts/build-visionos-device.sh
```

Signing remains local. Set `VISIONOS_DEVELOPMENT_TEAM` in an untracked
`CMakeUserPresets.json` or in Xcode; never commit a team ID, provisioning
profile, certificate, or credential.

## Current acceptance level

The native target is a prototype, not a downloadable release. Simulator and CI
compilation can prove source/build integration, but they cannot prove gaze and
pinch comfort, headset removal, spatial audio, thermals, focus behavior, or
long-session reliability. A physical Vision Pro must pass the V1 matrix before
the project describes the native build as supported.

Verified on 14 August 2026 with Xcode 26.5: the native arm64 simulator app and
the unsigned arm64 device app both compile and link; the simulator app installs,
launches, and presents the localized Gold-CD importer in a visionOS window.
Automated pointer injection can establish hover but does not constitute a
gaze-and-pinch acceptance test. Import, gameplay input, audio, resizing,
lifecycle, and comfort therefore remain physical-headset/manual gates.

The pinned SDL 2.32.10 checkout is prepared reproducibly in two stages:
`SDL2-ipados.patch` supplies the shared UIKit lifecycle and input work, then
`SDL2-visionos.patch` adds guarded visionOS window/Metal compatibility. The
visionOS preparation script is idempotent and applies both in that order.

The compatibility iPad build has reached startup, local-data recognition,
video, resizing, and input-event diagnostics in Vision Simulator. A live
mission plus the complete physical-headset matrix remains a manual gate.

See [the full Vision Pro concept](docs/VISIONOS-CONCEPT.md) and
[Apple-platform parity policy](docs/PLATFORM_PARITY.md).
