#!/usr/bin/env bash
set -euo pipefail

MAP_PATH="${1:-}"
EXPECT_LIB_ROOT="${2:-}"

has_rg=0
if command -v rg >/dev/null 2>&1; then
  has_rg=1
fi

contains_fixed() {
  local needle="$1"
  local path="$2"
  if [ "$has_rg" -eq 1 ]; then
    rg -q --fixed-strings "$needle" "$path"
  else
    grep -Fq -- "$needle" "$path"
  fi
}

unique_matches() {
  local pattern="$1"
  local path="$2"
  if [ "$has_rg" -eq 1 ]; then
    rg -o "$pattern" "$path" | sort -u
  else
    grep -Eo -- "$pattern" "$path" | sort -u
  fi
}

matching_lines() {
  local pattern="$1"
  local path="$2"
  if [ "$has_rg" -eq 1 ]; then
    rg -n "$pattern" "$path"
  else
    grep -En -- "$pattern" "$path"
  fi
}

if [ -z "$MAP_PATH" ] || [ ! -f "$MAP_PATH" ]; then
  echo "Usage: $0 <path-to-map-file> [expected-custom-lib-root]" >&2
  exit 2
fi

if [ -n "$EXPECT_LIB_ROOT" ]; then
  if ! contains_fixed "$EXPECT_LIB_ROOT/" "$MAP_PATH"; then
    echo "Expected custom libs root not found in map file: $EXPECT_LIB_ROOT" >&2
    exit 3
  fi
fi

FORBIDDEN_PATTERN='lib(btdm_app|bt|ble_mesh|mesh)\.a'
FOUND_LIBS="$(unique_matches "$FORBIDDEN_PATTERN" "$MAP_PATH" || true)"
if [ -n "$FOUND_LIBS" ]; then
  echo "Forbidden BT/mesh archives are still linked:" >&2
  echo "$FOUND_LIBS" >&2
  echo "" >&2
  echo "First matching map lines:" >&2
  { matching_lines "$FORBIDDEN_PATTERN" "$MAP_PATH" || true; } | head -n 40 >&2
  exit 4
fi

echo "No forbidden BT/mesh archives linked."
if [ -n "$EXPECT_LIB_ROOT" ]; then
  echo "Custom libs root verified: $EXPECT_LIB_ROOT"
fi
