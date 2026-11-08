#!/bin/sh
set -eu
cd "$(dirname "$0")/site"
echo "COFFEE_GRAINS playtest: http://127.0.0.1:8765/ (Ctrl-C to stop)"
exec python3 -m http.server 8765 --bind 127.0.0.1
