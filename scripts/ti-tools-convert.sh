#!/usr/bin/env bash
set -euo pipefail

if ! command -v ti-tools >/dev/null 2>&1; then
  echo "ti-tools not found. Install it first (see README)." >&2
  exit 1
fi

if [ $# -lt 2 ]; then
  echo "Usage: $0 <input> <output> [ti-tools args...]" >&2
  exit 2
fi

input="$1"
output="$2"
shift 2

ti-tools convert "$input" -o "$output" "$@"
