#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
work="${XDG_CACHE_HOME:-$HOME/.cache}/openroll/preview-0.17.2"
mkdir -p -- "$work"
sdk="$work/vendor/usr/lib64/blackmagic/BlackmagicRAWSDK"
profile="$work/vendor/BlackmagicRAW/BlackmagicRAWSpeedTest/profile.braw"

if [[ ! -f "$sdk/Linux/Include/BlackmagicRawAPI.h" ||
      ! -f "$sdk/Linux/Libraries/libBlackmagicRawAPI.so" || ! -f "$profile" ]]; then
    if [[ ${1:-} != *.tar || ! -r "${1:-}" ]]; then
        printf 'Pass your Blackmagic RAW 6.0 tar as the first argument.\n' >&2
        exit 2
    fi
    staging=$(mktemp -d "$work/sdk.XXXXXX")
    trap 'rm -rf -- "$staging"' EXIT
    "$source_dir/scripts/extract-sdk.sh" "$1" "$staging/vendor"
    mv -T -- "$staging/vendor" "$work/vendor"
fi
if [[ ${1:-} == *.tar ]]; then shift; fi

build="$work/build"
if [[ ! -f "$build/build.ninja" ]]; then
    meson setup "$build" "$source_dir" --buildtype=release -Dsdk_root="$sdk" -Dbraw_test_clip="$profile"
fi
meson compile -C "$build"
meson test -C "$build" --print-errorlogs
export OPENROLL_BRAW_RUNTIME="$sdk/Linux/Libraries"
exec "$build/openroll" "$@"
