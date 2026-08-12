#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdl_directory="$repository_root/third_party/SDL2"
patch_file="$repository_root/patches/SDL2-ipados.patch"

if [ ! -f "$sdl_directory/CMakeLists.txt" ]; then
    echo "SDL2 is missing. Run: git submodule update --init --recursive" >&2
    exit 1
fi

if git -C "$sdl_directory" apply --reverse --check "$patch_file" >/dev/null 2>&1; then
    echo "Tiberian Dawn for iPad SDL2 iPadOS patch is already applied."
    exit 0
fi

if ! git -C "$sdl_directory" apply --check "$patch_file"; then
    echo "The SDL2 checkout does not match release-2.32.10 or contains conflicting changes." >&2
    exit 1
fi

git -C "$sdl_directory" apply "$patch_file"
echo "Applied the Tiberian Dawn for iPad SDL2 iPadOS patch."
