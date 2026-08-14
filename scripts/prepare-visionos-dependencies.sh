#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdl_directory="$repository_root/third_party/SDL2"
patch_file="$repository_root/patches/SDL2-visionos.patch"

if git -C "$sdl_directory" apply --reverse --check "$patch_file" >/dev/null 2>&1; then
    echo "Tiberian Dawn SDL2 iPadOS and native visionOS patches are already applied."
    exit 0
fi

"$repository_root/scripts/prepare-ipados-dependencies.sh"

if ! git -C "$sdl_directory" apply --check "$patch_file"; then
    echo "The SDL2 checkout does not match the prepared iPadOS base or contains conflicting visionOS changes." >&2
    exit 1
fi

git -C "$sdl_directory" apply "$patch_file"
echo "Applied the Tiberian Dawn native visionOS SDL2 patch."
