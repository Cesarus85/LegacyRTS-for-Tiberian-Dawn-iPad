# Tiberian Dawn for macOS

> **EA has not endorsed and does not support this product.**

The macOS target is a self-contained Universal 2 application for Apple Silicon
and Intel Macs running macOS 11 or newer. It contains source-built SDL2 and uses
only system runtime frameworks; Homebrew is not required to run the resulting
app.

The repository and app contain no original game data. You need the original
C&C Gold GDI and Nod disc images that you are legally entitled to use.

## Build the app

Requirements:

- macOS with Xcode and the macOS SDK installed
- CMake 3.25 or newer
- internet access only when the pinned SDL2 submodule is not present

Clone or download the repository, then double-click:

```text
Build Tiberian Dawn for macOS.command
```

The builder verifies its tools and pinned SDL2 revision, compiles both `arm64`
and `x86_64`, rejects Homebrew runtime links, ad-hoc signs the local bundle, and
creates:

```text
dist/Tiberian-Dawn-macOS-universal.zip
```

It does not request administrator access, Apple credentials, or game data.

## First launch and game-data import

Unzip the archive and open `Tiberian Dawn.app`. When no valid data is installed,
the bilingual app asks you to select the GDI and Nod C&C Gold ISO files together
or one after the other. It can alternatively import one prepared `vanillatd`
directory. Selection, validation, extraction, and installation happen locally;
the ISOs are not copied or uploaded.

The game opens in a resizable Retina-aware window so the pointer remains easy
to control. The green window button can enlarge the window; Option-Return
toggles the engine's full-screen mode.

Immutable game data is stored below:

```text
~/Library/Application Support/Tiberian Dawn/Game Data/vanillatd
```

Settings and saves are kept separately below:

```text
~/Library/Application Support/Tiberian Dawn/User Data/vanillatd
```

The importer does not support the Remastered Collection, The First Decade, or
The Ultimate Collection.

## Signing and distribution

Local source builds are ad-hoc signed and can be opened normally on the Mac
that built them. A downloadable precompiled binary must be signed with the
publisher's Apple Developer ID certificate and notarized by Apple. Those
credentials are intentionally not stored in this repository. The checked-in
builder therefore remains the supported and reproducible distribution method.

## Manual build

```sh
git submodule update --init --recursive
./scripts/package-macos-app.sh
```
