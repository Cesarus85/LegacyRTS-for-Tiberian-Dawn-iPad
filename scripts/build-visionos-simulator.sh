#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
developer_dir=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}

"$project_dir/scripts/prepare-visionos-dependencies.sh"
DEVELOPER_DIR="$developer_dir" cmake --preset visionos-simulator -S "$project_dir"
DEVELOPER_DIR="$developer_dir" cmake --build "$project_dir/build/visionos-simulator" \
    --config Debug \
    --target TiberianDawn \
    -- -quiet CODE_SIGNING_ALLOWED=NO
