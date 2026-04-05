#!/bin/bash
# This whole file was NOT made by Claude btw (although I did use Claude for some cleanup)

set -e
trap 'echo "[+] Cleanup..." && rm -rf temp' EXIT # For example this was Claude's idea

# In case I'm stupid and forget an argument (also Claude's idea)
if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: ./new_reaction.sh <url> <name> [start] [end]"
    exit 1
fi

FILE="$2.ogg"

# Download video
echo "[+] Downloading video..."
yt-dlp -x --audio-format vorbis -o "temp/pre_$2.%(ext)s" "$1"

# Cutting (optional)
if [[ -n "$3" && -n "$4" ]]; then
    echo "[+] Cutting..."
    echo "[+] Start: $3"
    echo "[+] End: $4"

    mv "temp/pre_$FILE" "temp/uncut_$FILE"
    ffmpeg -i "temp/uncut_$FILE" -ss "$3" -to "$4" -c libvorbis -q:a 4 "temp/pre_$FILE"
    echo "[+] Sucessfully cut the audio"
fi

# Normalize audio
echo "[+] Normalizing..."
ffmpeg -i "temp/pre_$FILE" -filter:a "loudnorm=I=-5:TP=-1.5:LRA=11" -c:a libvorbis -q:a 4 "temp/n_$FILE"
ffmpeg -i "temp/n_$FILE" -filter:a "volume=1.3" -c:a libvorbis -q:a 4 "$FILE"

# Check loudness
output=$(ffmpeg -i "$FILE" -filter:a volumedetect -f null /dev/null 2>&1)
mean=$(echo "$output" | grep "mean_volume" | awk '{print $5}')
max=$(echo "$output" | grep "max_volume" | awk '{print $5}')

echo "Volume metrics:"
echo "mean_volume: $mean"
echo "max_volume: $max"
printf "\nValues may be tweaked manually if they do not fit\n"