#!/bin/sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repository_root"

failure=0
for path in $(git ls-files); do
    lower_path=$(printf '%s' "$path" | tr '[:upper:]' '[:lower:]')
    case "$lower_path" in
        *.iso|*.cue|*.mix|*.aud|*.vqa|*.sav|*.ipa|*.mobileprovision|*.p12|*.cer|*.pem|*.key|autosave.ipad*)
            echo "Forbidden release file: $path" >&2
            failure=1
            ;;
    esac
done

if git grep -I -n -E 'BEGIN (RSA |EC |DSA |OPENSSH )?PRIVATE KEY' -- . ':!scripts/check-release-tree.sh'; then
    echo "A private-key marker is present in tracked source." >&2
    failure=1
fi

if git grep -I -n -E 'IPADOS_DEVELOPMENT_TEAM[^[:cntrl:]]*[A-Z0-9]{10}' -- . ':!scripts/check-release-tree.sh'; then
    echo "A concrete Apple development team is present in tracked source." >&2
    failure=1
fi

if [ "$failure" -ne 0 ]; then
    exit 1
fi

echo "Release tree contains no known game-data, signing, or private-key files."
