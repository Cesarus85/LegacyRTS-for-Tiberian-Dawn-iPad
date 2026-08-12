# Legacy RTS — for Command & Conquer: Tiberian Dawn on iPad

> **EA has not endorsed and does not support this product.**

**Command & Conquer: Tiberian Dawn (1995), running natively on iPadOS** through
an unofficial, community-made engine port. Legacy RTS is based on
[Vanilla Conquer](https://github.com/TheAssemblyArmada/Vanilla-Conquer) and the
source code released by Electronic Arts.

> **Independent modified project.** Legacy RTS is not an Electronic Arts
> product and is not affiliated with Electronic Arts. Command & Conquer and
> related names are Electronic Arts trademarks. They are used in plain text
> only to identify the game this port is for and the data it is compatible with.

The repository contains **source code only**. It does not contain the game,
disc images, movies, music, graphics, or other original game data. You must
supply data from C&C Gold GDI and Nod discs that you are legally entitled to
use.

[Project page](https://cesarus85.github.io/LegacyRTS-for-Tiberian-Dawn-iPad/) · [iPad build guide](README-iPadOS.md) ·
[Roadmap](IPADOS-ROADMAP.md) · [License](License.txt)

## Highlights

- Native iPad window lifecycle, rotation, safe areas, and Stage Manager layout
- Touch-first RTS controls, mouse and trackpad, hardware keyboard, controllers,
  and Apple Pencil support
- Guided on-device import of the GDI and Nod C&C Gold disc images
- Files-visible manual saves plus transactional interruption recovery
- 60 FPS presentation with a persistent 30 FPS battery mode
- Metal-accelerated SDL rendering with full-size and pixel-perfect modes
- iPadOS audio-session handling for interruptions and route changes
- No analytics, accounts, advertising, or network multiplayer

The original campaigns, missions, videos, audio, and languages come from the
user-supplied game data. Multiplayer is currently unavailable.

## What this port involved

The original engine assumes a desktop window, mouse, writable working
directory, long-lived foreground process, and desktop audio device. The iPad
port adds the platform behavior around that engine while preserving its game
logic and original 640 × 400 presentation:

- UIKit scene lifecycle and safe-area-aware SDL windows for rotation and Stage
  Manager
- A touch gesture state machine that distinguishes taps, selection drags,
  two-finger map movement, long presses, cancellation, and Pencil input
- Sandboxed paths for imported assets, settings, saves, and recovery state
- Atomic two-disc import with validation, storage checks, and actionable errors
- Render suspension, dirty-frame presentation, battery and thermal policies
- iPad audio-session coordination and interruption-safe mission recovery

This was developed as a human + AI collaboration: product direction, device
testing, visual judgment, and release decisions are human-led; implementation,
diagnostics, documentation, and repetitive validation were AI-assisted.

## Current status

The physical-iPad build starts, imports complete C&C Gold GDI and Nod data,
plays campaigns and live missions, saves and resumes, rotates, and works in
Stage Manager. Touch, Pencil, mouse/trackpad, keyboard, and controller paths are
implemented. The app remains a source-built development project rather than an
App Store or pre-signed IPA distribution.

## Build for a physical iPad

Requirements:

- A Mac with Xcode and the iOS platform components installed
- CMake 3.25 or newer
- An Apple development team selected for code signing
- Git with submodule support

Clone the project and its SDL dependency:

```sh
git clone --recurse-submodules https://github.com/Cesarus85/LegacyRTS-for-Tiberian-Dawn-iPad.git
cd LegacyRTS-for-Tiberian-Dawn-iPad
./scripts/prepare-ipados-dependencies.sh
```

Create your private signing preset:

```sh
cp resources/CMakeUserPresets.json.example CMakeUserPresets.json
```

Edit the `IPADOS_BUNDLE_IDENTIFIER` and `IPADOS_DEVELOPMENT_TEAM` placeholders,
then configure and build:

```sh
cmake --preset ipados-device-local
cmake --build --preset ipados-device-local
open build/ipados-device/VanillaConquer.xcodeproj
```

In Xcode, select the `VanillaTD` scheme and your connected iPad. The detailed
[iPadOS guide](README-iPadOS.md) covers installation, first launch, data import,
controls, storage, and diagnostics.

## Game-data import

On first launch, choose the original C&C Gold **GDI and Nod ISO files together**
in the system document picker. The app validates both discs, extracts the data
locally, and does not retain the ISO files. A previously prepared data directory
can be selected instead.

The importer does not support data from the Remastered Collection, The First
Decade, or The Ultimate Collection. No game data may be submitted to this
repository in issues, pull requests, test fixtures, or releases.

## macOS status

The upstream desktop path remains available and is checked for macOS build
portability. It is separate from the primary iPad product: the native iPad
importer, Files integration, touch controls, and iPad lifecycle are compiled
only for iPadOS. A portable macOS app also needs its SDL2 and OpenAL dynamic
libraries bundled and should not be advertised as a supported release until it
has completed a launch and gameplay smoke test.

## Known limitations

- Multiplayer and networking are disabled.
- Red Alert is present in the upstream source tree but is not an iPad target.
- External-display behavior is not release-tested.
- The macOS target compiles, but portable packaging and live gameplay still
  need explicit release validation.
- Original game limitations and content are unchanged; this project does not
  provide replacement assets or translations.

## Project lineage and credits

- Westwood Studios and Electronic Arts — the original game and released source
- [Electronic Arts C&C source release](https://github.com/electronicarts/CnC_Remastered_Collection)
- [Vanilla Conquer](https://github.com/TheAssemblyArmada/Vanilla-Conquer) — the
  portable engine foundation used by this fork
- SDL contributors — platform, rendering, and input foundation
- unshieldv3 contributors — InstallShield archive extraction used by the guided
  importer
- Legacy RTS contributors — iPadOS integration, controls, importer, lifecycle,
  storage, performance, and documentation

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for license details.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request.
Keep changes focused, mark modified source clearly, and never include original
game data or confidential signing material. Security reports belong in the
private channel described in [SECURITY.md](SECURITY.md).

## License and notices

The code is distributed under GNU GPL v3 or later with the permitted additional
terms in [License.txt](License.txt). Those additional terms include trademark,
attribution, modification-marking, and warranty provisions. See
[NOTICE.md](NOTICE.md) for the modification statement and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for bundled dependencies.

No warranty is provided. This repository is intended for preservation,
portability research, and lawful personal use of independently supplied game
data.
