#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_file="$repository_root/build/ipados-device/TiberianDawnForiPad.xcodeproj"
sdl_directory="$repository_root/third_party/SDL2"
sdl_commit="5d249570393f7a37e037abf22cd6012a4cc56a71"
temporary_checkout=""
open_project=true

case "${1:-}" in
    "") ;;
    --no-open) open_project=false ;;
    *) printf 'Usage: %s [--no-open]\n' "$0" >&2; exit 2 ;;
esac

finish()
{
    status=$?
    if [ -n "$temporary_checkout" ] && [ -d "$temporary_checkout" ]; then
        rm -rf -- "$temporary_checkout"
    fi
    if [ "$status" -ne 0 ]; then
        printf '\nSetup stopped. Read the message above, then press Return to close this window.\n'
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
    for candidate in \
        "$path_cmake" \
        "/Applications/CMake.app/Contents/bin/cmake" \
        "/opt/homebrew/bin/cmake" \
        "/usr/local/bin/cmake"
    do
        if [ -n "$candidate" ] && [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

printf '\nTiberian Dawn for iPad — safe Xcode setup\n'
printf 'No administrator access, Apple credentials, or game data are requested.\n\n'

[ "$(uname -s)" = "Darwin" ] || fail "This setup must be run on a Mac."

selected_developer_dir=$(/usr/bin/xcode-select -p 2>/dev/null || true)
if [ -n "${DEVELOPER_DIR:-}" ] && [ -d "$DEVELOPER_DIR" ]; then
    developer_dir="$DEVELOPER_DIR"
elif [ -n "$selected_developer_dir" ] && [ "${selected_developer_dir#*Xcode.app/}" != "$selected_developer_dir" ]; then
    developer_dir="$selected_developer_dir"
elif [ -d "/Applications/Xcode.app/Contents/Developer" ]; then
    developer_dir="/Applications/Xcode.app/Contents/Developer"
else
    fail "Install Xcode from Apple, open it once, and accept its license before running this setup."
fi

if ! DEVELOPER_DIR="$developer_dir" /usr/bin/xcrun --sdk iphoneos --show-sdk-path >/dev/null 2>&1; then
    fail "Xcode's iPhoneOS components are unavailable. Open Xcode, finish installing components, and accept its license."
fi
printf '✓ Xcode and the iPhoneOS SDK are available.\n'

cmake_bin=$(find_cmake || true)
[ -n "$cmake_bin" ] || fail "CMake 3.25 or newer is required. Install the signed macOS application from https://cmake.org/download/ and run this file again."
cmake_version=$($cmake_bin --version | awk 'NR == 1 { print $3 }')
cmake_major=${cmake_version%%.*}
cmake_remainder=${cmake_version#*.}
cmake_minor=${cmake_remainder%%.*}
case "$cmake_major.$cmake_minor" in
    *[!0-9.]*) fail "Could not read the installed CMake version." ;;
esac
if [ "$cmake_major" -lt 3 ] || { [ "$cmake_major" -eq 3 ] && [ "$cmake_minor" -lt 25 ]; }; then
    fail "CMake $cmake_version is too old. Install CMake 3.25 or newer from https://cmake.org/download/."
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
        temporary_checkout=$(mktemp -d "${TMPDIR:-/tmp}/tiberian-dawn-sdl2.XXXXXX")
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
[ -f "$sdl_directory/CMakeLists.txt" ] || fail "SDL2 could not be prepared. Check the network connection and try again."

actual_sdl_commit=$(git -C "$sdl_directory" rev-parse HEAD 2>/dev/null || true)
[ "$actual_sdl_commit" = "$sdl_commit" ] || fail "SDL2 is not at the project-pinned revision. Remove third_party/SDL2 and run this setup again."
printf '✓ The pinned SDL2 source is present.\n'

"$repository_root/scripts/prepare-ipados-dependencies.sh"

printf '\nGenerating the current physical-iPad Xcode project...\n'
DEVELOPER_DIR="$developer_dir" "$cmake_bin" --preset ipados-device -S "$repository_root"
[ -d "$project_file" ] || fail "CMake finished without creating the expected Xcode project."

printf '\n✓ Xcode project created successfully.\n'
printf '\nIn Xcode:\n'
printf '  1. Select the TiberianDawn scheme.\n'
printf '  2. Under Signing & Capabilities, choose your Apple Development Team.\n'
printf '  3. If Xcode requests it, use a unique Bundle Identifier.\n'
printf '  4. Select your connected iPad and press Run.\n\n'

if [ "$open_project" = true ]; then
    /usr/bin/open "$project_file"
fi
trap - EXIT HUP INT TERM
exit 0
