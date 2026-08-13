# Contributing

The iPadOS and macOS apps must remain at feature parity wherever Apple APIs
allow it. Read [docs/PLATFORM_PARITY.md](docs/PLATFORM_PARITY.md) before changing
shared gameplay, graphics, audio, menus, localization, saves, or resources.

Thank you for helping improve Tiberian Dawn for iPad and macOS.

## Before submitting a change

1. Keep the project name and visual identity independent from Electronic Arts.
2. Do not add original game data, screenshots containing personal information,
   disc images, extracted assets, signing certificates, provisioning profiles,
   Apple team IDs, or credentials.
3. Preserve existing copyright headers and mark substantial modifications.
4. Build the physical-iPad target for iPad-specific work and the Universal 2
   package for macOS work. Run the host tests for shared engine changes.
5. Describe what changed, how it was tested, and any remaining device-specific
   validation.

Bug reports may name the original game to identify compatible data, but must
not attach its files. Logs should be checked for usernames, container paths,
device identifiers, and other private information before posting.

By contributing, you agree that your contribution is distributed under the
terms in [License.txt](License.txt).
