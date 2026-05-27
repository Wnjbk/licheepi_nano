#!/bin/sh
set -e

FILE=src/gmenu2x.cpp

if [ ! -f "$FILE" ]; then
echo "run this in gmenu2x source root"
exit 1
fi

if grep -q '#ifndef F1C200S_BUILD' "$FILE" && grep -q 'SDL_AddTimer(1000, hwCheck, NULL)' "$FILE"; then
echo "hwCheck already guarded"
exit 0
fi

python3 - <<'PY'
from pathlib import Path
p = Path('src/gmenu2x.cpp')
s = p.read_text()
needle = '\tSDL_TimerID hwCheckTimer = SDL_AddTimer(1000, hwCheck, NULL);'
replacement = '#ifndef F1C200S_BUILD\n\tSDL_TimerID hwCheckTimer = SDL_AddTimer(1000, hwCheck, NULL);\n#endif'
if needle not in s:
    raise SystemExit('SDL_AddTimer hwCheck line not found')
s = s.replace(needle, replacement, 1)
p.write_text(s)
PY

echo "hwCheck disabled for F1C200S_BUILD"