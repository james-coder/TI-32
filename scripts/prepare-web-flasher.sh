#!/usr/bin/env bash
set -euo pipefail

FQBN="${FQBN:-esp32:esp32:XIAO_ESP32C3}"
BUILD_DIR="${BUILD_DIR:-/tmp/ti32-webflash-build}"
OUT_DIR="${OUT_DIR:-web-flasher/firmware}"
LIBS_FILE="${LIBS_FILE:-esp32/ld_libs.no_bt_mesh}"

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli not found. Install it first (see README)." >&2
  exit 1
fi

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

EXTRA_BUILD_PROPS=()
if [ -f "$LIBS_FILE" ]; then
  EXTRA_BUILD_PROPS+=(--build-property "compiler.c.elf.libs=@${LIBS_FILE}")
fi

arduino-cli compile --fqbn "$FQBN" --output-dir "$BUILD_DIR" "${EXTRA_BUILD_PROPS[@]}" esp32

bootloader=$(ls -1 "$BUILD_DIR"/*.bootloader.bin 2>/dev/null | head -n1 || true)
partitions=$(ls -1 "$BUILD_DIR"/*.partitions.bin 2>/dev/null | head -n1 || true)
app=$(ls -1 "$BUILD_DIR"/*.bin 2>/dev/null | grep -v -E '(bootloader|partitions)' | head -n1 || true)

if [[ -z "$bootloader" || -z "$partitions" || -z "$app" ]]; then
  echo "Missing build outputs in $BUILD_DIR" >&2
  exit 2
fi

mkdir -p "$OUT_DIR"
cp "$bootloader" "$OUT_DIR/bootloader.bin"
cp "$partitions" "$OUT_DIR/partitions.bin"
cp "$app" "$OUT_DIR/firmware.bin"

echo "Copied firmware to $OUT_DIR. Update web-flasher/manifest.json if needed."
