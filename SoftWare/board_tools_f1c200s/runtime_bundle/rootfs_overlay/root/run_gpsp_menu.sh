#!/bin/sh

cd /root || exit 1

modprobe fb_st7789v 2>/dev/null

export SDL_VIDEODRIVER=fbcon
export SDL_FBDEV=/dev/fb1
export SDL_AUDIODRIVER=alsa
export AUDIODEV=plughw:1,0
export SDL_NOMOUSE=1

GPSP=/root/gpsp
CFG=/root/game_config.txt
ROM_DIR=/root
TMP=/tmp/gpsp_roms.$$

make_game_config()
{
rom="$1"

if [ ! -f "$rom" ]; then
echo "ROM not found: $rom"
return 1
fi

title=$(dd if="$rom" bs=1 skip=160 count=12 2>/dev/null | tr -d '\000')
code=$(dd if="$rom" bs=1 skip=172 count=4 2>/dev/null | tr -d '\000')
maker=$(dd if="$rom" bs=1 skip=176 count=2 2>/dev/null | tr -d '\000')

if [ -z "$title" ] || [ -z "$code" ] || [ -z "$maker" ]; then
echo "bad GBA header, cannot create game_config.txt"
return 1
fi

flash128=0
if strings "$rom" 2>/dev/null | grep -q 'FLASH1M'; then
flash128=1
fi

rm -f "$CFG"
printf 'game_name = %s\r\n' "$title" > "$CFG"
printf 'game_code = %s\r\n' "$code" >> "$CFG"
printf 'vender_code = %s\r\n' "$maker" >> "$CFG"

if [ "$code" = "AX4E" ]; then
printf 'idle_loop_eliminate_target = 08000732\r\n' >> "$CFG"
fi

if [ "$flash128" = "1" ]; then
printf 'flash_rom_type = 128KB\r\n' >> "$CFG"
fi

echo "game_config.txt generated: $title / $code / $maker"
if [ "$flash128" = "1" ]; then
echo "flash_rom_type = 128KB"
fi
}

choose_rom()
{
rm -f "$TMP"
find "$ROM_DIR" -maxdepth 2 -type f \( -iname '*.gba' -o -iname '*.zip' \) | sort > "$TMP"

count=$(wc -l < "$TMP")
if [ "$count" -eq 0 ]; then
echo "No .gba or .zip ROM found in $ROM_DIR"
rm -f "$TMP"
exit 1
fi

echo "Select ROM:"
n=1
while IFS= read -r rom; do
echo "$n) $rom"
n=$((n + 1))
done < "$TMP"

printf 'Input number: '
read num

case "$num" in
''|*[!0-9]*)
echo "bad selection"
rm -f "$TMP"
exit 1
;;
esac

rom=$(sed -n "${num}p" "$TMP")
rm -f "$TMP"

if [ -z "$rom" ]; then
echo "bad selection"
exit 1
fi

echo "$rom"
}

if [ -n "$1" ]; then
ROM="$1"
case "$ROM" in
/*) ;;
*) ROM="/root/$ROM" ;;
esac
else
ROM=$(choose_rom)
fi

make_game_config "$ROM"

if [ ! -x "$GPSP" ]; then
echo "gpsp not executable: $GPSP"
exit 1
fi

exec "$GPSP" "$ROM"
