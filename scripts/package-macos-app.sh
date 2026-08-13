#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
developer_dir=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}
build_root="$repository_root/build/macos-universal"
project_file="$build_root/TiberianDawnApple.xcodeproj"
source_app="$build_root/Release/Tiberian Dawn.app"
dist_root="$repository_root/dist"
archive="$dist_root/Tiberian-Dawn-macOS-universal.zip"
derived_data="${TMPDIR:-/tmp}/TiberianDawn-macOS-DerivedData"
package_work=""

finish()
{
    status=$?
    if [ -n "$package_work" ] && [ -d "$package_work" ]; then
        rm -rf -- "$package_work"
    fi
    exit "$status"
}
trap finish EXIT HUP INT TERM

fail()
{
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

[ "$(uname -s)" = "Darwin" ] || fail "The macOS app can be packaged only on a Mac."
command -v cmake >/dev/null 2>&1 || fail "CMake 3.25 or newer is required."
[ -x "$developer_dir/usr/bin/xcodebuild" ] || fail "Install Xcode in /Applications or set DEVELOPER_DIR."
[ -f "$repository_root/third_party/SDL2/CMakeLists.txt" ] || fail "Initialize the pinned SDL2 submodule first."

# Use the full Xcode toolchain for this process without changing the user's
# global xcode-select setting or requiring administrator access.
export DEVELOPER_DIR="$developer_dir"

printf 'Configuring the universal macOS app...\n'
cmake --preset macos-universal -S "$repository_root"
printf 'Building for Apple Silicon and Intel...\n'
/usr/bin/xcrun xcodebuild \
    -project "$project_file" \
    -scheme TiberianDawn \
    -configuration Release \
    -destination "generic/platform=macOS" \
    -derivedDataPath "$derived_data" \
    SYMROOT="$build_root" \
    ARCHS="arm64 x86_64" \
    ONLY_ACTIVE_ARCH=NO \
    CODE_SIGNING_ALLOWED=NO \
    CODE_SIGNING_REQUIRED=NO \
    -quiet
[ -d "$source_app" ] || fail "The build did not create the expected app bundle."

mkdir -p "$dist_root"
rm -f -- "$archive"
package_work=$(mktemp -d "${TMPDIR:-/tmp}/tiberian-dawn-macos-package.XXXXXX")
packaged_app="$package_work/Tiberian Dawn.app"
/usr/bin/ditto "$source_app" "$packaged_app"
# Downloaded artwork can carry Finder/provenance metadata that code signing
# rightfully rejects. It is not part of the application payload.
/usr/bin/xattr -cr "$packaged_app"

executable="$packaged_app/Contents/MacOS/Tiberian Dawn"
[ -x "$executable" ] || fail "The app bundle has no executable."
architectures=$(/usr/bin/lipo -archs "$executable")
case " $architectures " in
    *" arm64 "*) ;;
    *) fail "The packaged app is missing Apple Silicon support." ;;
esac
case " $architectures " in
    *" x86_64 "*) ;;
    *) fail "The packaged app is missing Intel support." ;;
esac

if /usr/bin/otool -L "$executable" | /usr/bin/grep -E '/opt/homebrew|/usr/local' >/dev/null; then
    fail "The app still contains a Homebrew runtime dependency."
fi

# Local source builds have no Developer ID identity. Ad-hoc signing seals the
# complete self-contained bundle; a public binary release must additionally be
# Developer ID signed and notarized by its publisher.
/usr/bin/codesign --force --deep --sign - --timestamp=none "$packaged_app"
/usr/bin/codesign --verify --deep --strict "$packaged_app"
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$packaged_app" "$archive"

printf '\nCreated:\n  %s\n' "$archive"
printf 'Architectures: %s\n' "$architectures"
printf 'Runtime libraries are self-contained or supplied by macOS.\n'
trap - EXIT HUP INT TERM
finish
