#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
developer_dir=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}

DEVELOPER_DIR="$developer_dir" cmake --build "$project_dir/build/ipados-simulator" \
    --config Debug \
    --target VanillaTD \
    -- -quiet CODE_SIGNING_ALLOWED=NO
