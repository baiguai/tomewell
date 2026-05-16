#!/usr/bin/env bash
set -euo pipefail

./build.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXE="$SCRIPT_DIR/tomewell"

if [[ ! -f "$EXE" ]]; then
    echo "Binary not found at $EXE"
    echo "Run ./build.sh first."
    exit 1
fi

exec "$EXE" "$@"
