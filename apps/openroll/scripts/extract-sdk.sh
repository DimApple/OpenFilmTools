#!/usr/bin/env bash
set -euo pipefail

if (( $# != 2 )); then
    echo "Usage: $0 /path/to/Blackmagic_RAW_Linux_6.0.tar /new/extraction-directory" >&2
    exit 2
fi

archive=$(realpath -- "$1")
destination=$(realpath -m -- "$2")
mkdir -- "$destination"
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

bsdtar -xf "$archive" -C "$work" 'Blackmagic RAW/blackmagic-raw-6.0.x86_64.rpm'     'Blackmagic RAW/BlackmagicRAW_6.0.tar.gz'
bsdtar -xf "$work/Blackmagic RAW/blackmagic-raw-6.0.x86_64.rpm" -C "$destination" '*/BlackmagicRAWSDK/*'
bsdtar -xf "$work/Blackmagic RAW/BlackmagicRAW_6.0.tar.gz" -C "$destination"     'BlackmagicRAW/BlackmagicRAWSpeedTest/profile.braw'
sdk="$destination/usr/lib64/blackmagic/BlackmagicRAWSDK"
test -f "$sdk/Linux/Include/BlackmagicRawAPI.h"
test -f "$sdk/Linux/Libraries/libBlackmagicRawAPI.so"
test -f "$destination/BlackmagicRAW/BlackmagicRAWSpeedTest/profile.braw"
printf '%s\n' "$sdk"
