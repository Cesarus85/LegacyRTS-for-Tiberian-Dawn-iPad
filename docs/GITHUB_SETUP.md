# GitHub repository setup

Suggested public repository metadata:

- **Repository name:** `Tiberian-Dawn-for-iPad-and-macOS`
- **Display title:** `Tiberian Dawn for iPad and macOS`
- **Description:** `Unofficial native iPadOS and macOS port for Command & Conquer: Tiberian Dawn (1995), with touch controls, adaptive resolution scaling, LAN and private-room multiplayer. No game assets included.`
- **Website:** use the GitHub Pages URL after the first Pages deployment
- **Topics:** `tiberian-dawn`, `command-and-conquer`, `ipad`, `ipados`, `ios`,
  `macos`, `universal-binary`, `rts`, `game-port`, `open-source-game`, `sdl2`, `metal`, `cmake`,
  `vanilla-conquer`

`Tiberian Dawn for iPad and macOS` remains the independent project name. The searchable wording
explicitly says that it is a port **for** the original game. The original title
appears in plain text in the repository metadata and README, not as the
project's logo or an assertion of affiliation. This follows EA's published
[Command & Conquer modding guidelines](https://www.ea.com/games/command-and-conquer/news/modding-faq),
including their required non-endorsement statement.

## Recommended settings

1. Create the repository without a generated README, license, or `.gitignore`.
2. Set the default branch to `main`.
3. Enable Issues and private vulnerability reporting.
4. Enable secret scanning and push protection.
5. Under **Pages → Build and deployment**, choose **GitHub Actions**.
6. Protect `main`: require the three CI jobs and disallow force pushes.
7. Keep Releases manual. The included workflow creates a draft only from an
   existing reviewed tag.

## Local remote layout

After the empty repository exists:

```sh
git remote rename origin upstream
git remote add origin https://github.com/Cesarus85/Tiberian-Dawn-for-iPad-and-macOS.git
git branch -M main
```

Do not run the push until the reviewed publication commit exists. `origin`
should point to the new project and `upstream` to Vanilla Conquer.
