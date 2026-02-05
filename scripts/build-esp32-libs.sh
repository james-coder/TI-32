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
  touch "$LIB_BUILDER_DIR/.gitconfig-empty"
  export GIT_CONFIG_GLOBAL="$LIB_BUILDER_DIR/.gitconfig-empty"
  export GIT_TERMINAL_PROMPT=0
  export GITHUB_REPOSITORY_OWNER="${LIB_BUILDER_AR_USER:-espressif}"
  if [ -n "${LIB_BUILDER_GITHUB_TOKEN:-}" ]; then
    export GITHUB_TOKEN="x-access-token:${LIB_BUILDER_GITHUB_TOKEN}"
  else
    unset GITHUB_TOKEN
  fi
  ./build.sh -t "$TARGET" -b idf-libs "$DEFCONFIG_NAME"

  IDF_PATH="$LIB_BUILDER_DIR/esp-idf"
  if [ -f "$IDF_PATH/export.sh" ]; then
    # shellcheck disable=SC1090
    source "$IDF_PATH/export.sh"
  fi

  ./build.sh -s -t "$TARGET" -b copy-bootloader "$DEFCONFIG_NAME"
  ./build.sh -s -t "$TARGET" -b mem-variant "$DEFCONFIG_NAME"
)

SRC_DIR="$LIB_BUILDER_DIR/out/tools/esp32-arduino-libs/$TARGET"
if [ ! -d "$SRC_DIR" ]; then
  echo "Missing built libs: $SRC_DIR" >&2
  exit 1
fi

# lib-builder sometimes skips bootloader outputs; fall back to the Arduino core copy.
DEFAULT_LIBS_DIR="${DEFAULT_LIBS_DIR:-}"
if [ -z "$DEFAULT_LIBS_DIR" ]; then
  DEFAULT_LIBS_DIR=$(ls -1d "$HOME"/.arduino15/packages/esp32/tools/${TARGET}-libs/* 2>/dev/null | sort -V | tail -n1 || true)
fi
if [ -n "$DEFAULT_LIBS_DIR" ] && [ -d "$DEFAULT_LIBS_DIR/bin" ]; then
  mkdir -p "$SRC_DIR/bin"
  for f in "$DEFAULT_LIBS_DIR"/bin/bootloader_*.elf; do
    [ -e "$f" ] || continue
    dst="$SRC_DIR/bin/$(basename "$f")"
    if [ ! -f "$dst" ]; then
      cp "$f" "$dst"
    fi
  done
fi
if [ -n "$DEFAULT_LIBS_DIR" ]; then
  if [ ! -f "$SRC_DIR/sdkconfig" ] && [ -f "$DEFAULT_LIBS_DIR/sdkconfig" ]; then
    cp "$DEFAULT_LIBS_DIR/sdkconfig" "$SRC_DIR/sdkconfig"
  fi
  for dir in "$DEFAULT_LIBS_DIR"/*_*; do
    [ -d "$dir" ] || continue
    base="$(basename "$dir")"
    case "$base" in
      *_qspi|*_opi)
        if [ ! -d "$SRC_DIR/$base" ]; then
          cp -R "$dir" "$SRC_DIR/$base"
        fi
        ;;
    esac
  done
  if [ -d "$DEFAULT_LIBS_DIR/lib" ]; then
    mkdir -p "$SRC_DIR/lib"
    for f in "$DEFAULT_LIBS_DIR"/lib/*.a; do
      [ -e "$f" ] || continue
      base="$(basename "$f")"
      case "$base" in
        libbt.a|libbtdm_app.a|libble_mesh.a|libmesh.a)
          continue
          ;;
      esac
      if [ ! -f "$SRC_DIR/lib/$base" ]; then
        cp "$f" "$SRC_DIR/lib/$base"
      fi
    done
  fi
  if [ -d "$DEFAULT_LIBS_DIR/ld" ]; then
    mkdir -p "$SRC_DIR/ld"
    for f in "$DEFAULT_LIBS_DIR"/ld/*; do
      [ -e "$f" ] || continue
      base="$(basename "$f")"
      if [ ! -f "$SRC_DIR/ld/$base" ]; then
        cp "$f" "$SRC_DIR/ld/$base"
      fi
    done
  fi
  if [ -d "$DEFAULT_LIBS_DIR/flags" ]; then
    mkdir -p "$SRC_DIR/flags"
    for f in "$DEFAULT_LIBS_DIR"/flags/*; do
      [ -e "$f" ] || continue
      base="$(basename "$f")"
      if [ ! -f "$SRC_DIR/flags/$base" ]; then
        cp "$f" "$SRC_DIR/flags/$base"
      fi
    done
  fi
fi
if [ ! -f "$SRC_DIR/bin/bootloader_qio_80m.elf" ]; then
  echo "Missing bootloader in built libs: $SRC_DIR/bin/bootloader_qio_80m.elf" >&2
  ls -la "$SRC_DIR/bin" 2>/dev/null || true
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
