#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${TARGET:-esp32c3}"
LIB_BUILDER_DIR="${LIB_BUILDER_DIR:-$ROOT_DIR/.cache/esp32-lib-builder}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR/.cache/esp32-arduino-libs}"
DEFCONFIG_NAME="${DEFCONFIG_NAME:-no_bt_mesh}"
DEFCONFIG_SRC="${DEFCONFIG_SRC:-$ROOT_DIR/esp32/lib-builder/defconfig.no_bt_mesh}"
LIB_BUILDER_REF="${LIB_BUILDER_REF:-}"

if [ ! -f "$DEFCONFIG_SRC" ]; then
  echo "Missing defconfig: $DEFCONFIG_SRC" >&2
  exit 1
fi

if [ ! -d "$LIB_BUILDER_DIR/.git" ]; then
  git clone https://github.com/espressif/esp32-arduino-lib-builder "$LIB_BUILDER_DIR"
else
  git -C "$LIB_BUILDER_DIR" fetch --all --tags
fi

if [ -n "$LIB_BUILDER_REF" ]; then
  git -C "$LIB_BUILDER_DIR" checkout "$LIB_BUILDER_REF"
else
  git -C "$LIB_BUILDER_DIR" checkout master || git -C "$LIB_BUILDER_DIR" checkout main
  git -C "$LIB_BUILDER_DIR" pull --ff-only || true
fi

cp "$DEFCONFIG_SRC" "$LIB_BUILDER_DIR/configs/defconfig.$DEFCONFIG_NAME"

(
  cd "$LIB_BUILDER_DIR"
  ./build.sh -t "$TARGET" -b idf-libs "$DEFCONFIG_NAME"
)

SRC_DIR="$LIB_BUILDER_DIR/out/tools/esp32-arduino-libs/$TARGET"
if [ ! -d "$SRC_DIR" ]; then
  echo "Missing built libs: $SRC_DIR" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
rm -rf "$OUT_DIR/$TARGET"
cp -R "$SRC_DIR" "$OUT_DIR/$TARGET"

echo "Custom ESP32 libs copied to: $OUT_DIR/$TARGET"
echo "Use with Arduino CLI:"
echo "  arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 \\"
echo "    --build-property runtime.tools.${TARGET}-libs.path=\"$OUT_DIR/$TARGET\" \\"
echo "    esp32"
