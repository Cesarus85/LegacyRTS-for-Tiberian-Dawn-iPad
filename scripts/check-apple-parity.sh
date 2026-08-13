#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo_root"

required_assets='resources/apple/ModernArt/manifest.ini
resources/apple/ModernArt/ui/cursor-default.png
resources/apple/ModernArt/units/buggy/buggy-atlas-4x.png
resources/apple/ModernArt/units/humvee/humvee-atlas-4x.png
resources/apple/ModernArt/units/minigunner/minigunner-atlas-4x.png'

printf '%s\n' "$required_assets" | while IFS= read -r asset; do
    test -f "$asset" || { echo "Missing shared Apple asset: $asset" >&2; exit 1; }
    references=$(grep -cF "$asset" tiberiandawn/CMakeLists.txt)
    test "$references" -ge 2 || {
        echo "Shared Apple asset is not packaged by both targets: $asset" >&2
        exit 1
    }
done

grep -q 'if(IPADOS_PORT OR MACOS_PORT)' common/CMakeLists.txt
grep -q '#if defined(IPADOS_PORT) || defined(MACOS_PORT)' tiberiandawn/visudlg.cpp
grep -q '#if defined(IPADOS_PORT) || defined(MACOS_PORT)' tiberiandawn/gamedlg.cpp
grep -q '"OPENAL": "OFF"' CMakePresets.json

if grep -R -n -E 'LegacyRTS|LEGACY_RTS' \
    --exclude=check-apple-parity.sh \
    --exclude-dir=.git --exclude-dir=build --exclude-dir=dist \
    --exclude-dir=third_party .; then
    echo "Obsolete LegacyRTS naming found" >&2
    exit 1
fi

echo "Apple platform parity checks passed."
