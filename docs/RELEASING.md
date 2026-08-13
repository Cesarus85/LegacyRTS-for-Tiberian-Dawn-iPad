# Release checklist

This checklist is for maintainers preparing a public source release.

## Repository

- [ ] The project repository is the `origin` remote; Vanilla Conquer is
      configured as `upstream`.
- [ ] SDL is checked out recursively and `scripts/prepare-ipados-dependencies.sh`
      applies cleanly.
- [ ] `git status` contains only intended source and documentation changes.
- [ ] No ISO, MIX, AUD, VQA, save, provisioning, certificate, or private key
      files are tracked.
- [ ] The repository's secret scanning and push protection are enabled.

## Quality

- [ ] Host tests pass.
- [ ] The Universal 2 macOS package builds and its extracted app passes
      architecture, runtime-link, and code-signature checks.
- [ ] The Release device configuration builds and signs.
- [ ] A clean install imports both Gold discs and reaches a live mission.
- [ ] Touch selection, pause/resume, recovery save, audio interruption, and
      manual save import/export have received a physical-device smoke test.

## Publication

- [ ] README, project page, notices, and known limitations match the release.
- [ ] The release contains source code only unless a separately reviewed binary
      distribution is intentionally being made.
- [ ] No original game assets are present in the tag, release notes, screenshots,
      or downloadable artifacts.
- [ ] The tag is signed or otherwise traceable to the reviewed commit.

The GitHub Pages workflow publishes `docs/` from `main`. The source-release
workflow is intentionally manual so creating a tag never publishes an
unreviewed binary automatically.
