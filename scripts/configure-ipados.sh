#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
developer_dir=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}

DEVELOPER_DIR="$developer_dir" cmake --preset ipados-simulator -S "$project_dir"

echo "Generated: $project_dir/build/ipados-simulator/TiberianDawnForiPad.xcodeproj"
