#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sdl_directory="$repository_root/third_party/SDL2"
sdl_commit="5d249570393f7a37e037abf22cd6012a4cc56a71"
temporary_checkout=""

finish()
{
    status=$?
    if [ -n "$temporary_checkout" ] && [ -d "$temporary_checkout" ]; then
        rm -rf -- "$temporary_checkout"
    fi
    if [ "$status" -ne 0 ]; then
        printf '\nBuild stopped. Read the message above, then press Return to close this window.\n'
        read -r _answer || true
    fi
    exit "$status"
}
trap finish EXIT
trap 'exit 130' HUP INT TERM

fail()
{
    printf '\nERROR: %s\n' "$1" >&2
    exit 1
}

find_cmake()
{
    path_cmake=$(command -v cmake 2>/dev/null || true)
    for candidate in "$path_cmake" "/Applications/CMake.app/Contents/bin/cmake" "/opt/homebrew/bin/cmake" "/usr/local/bin/cmake"
    do
        if [ -n "$candidate" ] && [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

printf '\nTiberian Dawn for macOS — universal app builder\n'
printf 'No administrator access, Apple credentials, or game data are requested.\n\n'
[ "$(uname -s)" = "Darwin" ] || fail "This builder must be run on a Mac."

developer_dir=$(/usr/bin/xcode-select -p 2>/dev/null || true)
if [ -z "$developer_dir" ] || ! DEVELOPER_DIR="$developer_dir" /usr/bin/xcrun --sdk macosx --show-sdk-path >/dev/null 2>&1; then
    fail "Install Xcode, open it once, and accept its license before running this builder."
fi
printf '✓ Xcode and the macOS SDK are available.\n'

cmake_bin=$(find_cmake || true)
[ -n "$cmake_bin" ] || fail "CMake 3.25 or newer is required. Install the signed macOS application from https://cmake.org/download/."
cmake_version=$($cmake_bin --version | awk 'NR == 1 { print $3 }')
cmake_major=${cmake_version%%.*}
cmake_remainder=${cmake_version#*.}
cmake_minor=${cmake_remainder%%.*}
if [ "$cmake_major" -lt 3 ] || { [ "$cmake_major" -eq 3 ] && [ "$cmake_minor" -lt 25 ]; }; then
    fail "CMake $cmake_version is too old. Install CMake 3.25 or newer."
fi
printf '✓ CMake %s is available.\n' "$cmake_version"

if [ ! -f "$sdl_directory/CMakeLists.txt" ]; then
    git_bin=$(command -v git 2>/dev/null || true)
    [ -n "$git_bin" ] || fail "Git is required to retrieve the declared SDL2 source dependency."
    if "$git_bin" -C "$repository_root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        printf 'Retrieving the pinned SDL2 submodule...\n'
        "$git_bin" -C "$repository_root" submodule update --init --recursive
    else
        printf 'Retrieving the pinned SDL2 source for this downloaded archive...\n'
        temporary_checkout=$(mktemp -d "${TMPDIR:-/tmp}/tiberian-dawn-macos-sdl2.XXXXXX")
        "$git_bin" clone --filter=blob:none --no-checkout https://github.com/libsdl-org/SDL.git "$temporary_checkout/SDL2"
        "$git_bin" -C "$temporary_checkout/SDL2" fetch --depth 1 origin "$sdl_commit"
        "$git_bin" -C "$temporary_checkout/SDL2" checkout --detach "$sdl_commit"
        mkdir -p "$repository_root/third_party"
        if [ -e "$sdl_directory" ]; then
            rmdir "$sdl_directory" 2>/dev/null || fail "third_party/SDL2 exists but is not a usable or empty dependency directory."
        fi
        mv "$temporary_checkout/SDL2" "$sdl_directory"
        rmdir "$temporary_checkout"
        temporary_checkout=""
    fi
fi

actual_sdl_commit=$(git -C "$sdl_directory" rev-parse HEAD 2>/dev/null || true)
[ "$actual_sdl_commit" = "$sdl_commit" ] || fail "SDL2 is not at the project-pinned revision."
printf '✓ The pinned SDL2 source is present.\n\n'

PATH="$(dirname -- "$cmake_bin"):$PATH" "$repository_root/scripts/package-macos-app.sh"
printf '\n✓ The portable app is ready. On first launch it guides you through selecting the GDI and Nod Gold ISOs.\n'
/usr/bin/open -R "$repository_root/dist/Tiberian-Dawn-macOS-universal.zip"
trap - EXIT HUP INT TERM
exit 0
