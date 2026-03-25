#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."
WEB_FRONTEND="$ROOT/../track-web/frontend"

if [ ! -d "$WEB_FRONTEND" ]; then
    echo "Error: track-web/frontend not found at $WEB_FRONTEND"
    echo "Make sure track-web is cloned next to track-embedded."
    exit 1
fi

echo "=== Building UI for Pi ==="

cd "$WEB_FRONTEND"
npm install --silent
npx vite build --mode pi --base=/app/ --outDir "$ROOT/captive-portal/ui" --emptyOutDir

echo "=== UI built to captive-portal/ui/ ==="
