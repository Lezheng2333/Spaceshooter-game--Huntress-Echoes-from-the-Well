#!/bin/bash
# render_readme.sh <html> <out.png> <width> <height>
set -euo pipefail
HTML_ABS="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
OUT_ABS="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
W="$3"; H="$4"
PY="${DSH_PY:-/Users/song_jiarui/.dsh/dsh-runtimes/dsh-primary-runtime/dependencies/python/bin/python3}"
PROFILE="$(mktemp -d)"; RAW="$(mktemp -d)/raw.png"
URL=$("$PY" - "$HTML_ABS" <<'PY'
import pathlib,sys
print(pathlib.Path(sys.argv[1]).as_uri())
PY
)
"/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge" \
  --headless=new --no-sandbox --disable-gpu --disable-software-rasterizer \
  --disable-dev-shm-usage --no-first-run --no-default-browser-check --hide-scrollbars \
  --force-device-scale-factor=2 --user-data-dir="$PROFILE" \
  --window-size="$W,$H" --screenshot="$RAW" "$URL" >/dev/null 2>&1
"$PY" - "$RAW" "$OUT_ABS" "$W" "$H" <<'PY'
from PIL import Image
import sys
im=Image.open(sys.argv[1]).convert('RGB')
im=im.resize((int(sys.argv[3]),int(sys.argv[4])), Image.LANCZOS)
im.save(sys.argv[2], optimize=True)
print('wrote', sys.argv[2], im.size)
PY
rm -rf "$PROFILE" "$(dirname "$RAW")"
