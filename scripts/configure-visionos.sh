#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
developer_dir=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}

"$project_dir/scripts/prepare-visionos-dependencies.sh"
DEVELOPER_DIR="$developer_dir" cmake --preset visionos-simulator -S "$project_dir"

echo "Generated: $project_dir/build/visionos-simulator/TiberianDawnApple.xcodeproj"
