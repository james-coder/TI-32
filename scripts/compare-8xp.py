#!/usr/bin/env python3
import sys
from pathlib import Path

COMMENT_START = 11
COMMENT_LEN = 42
COMMENT_END = COMMENT_START + COMMENT_LEN


def mask_comment(data: bytes) -> bytes:
    if len(data) < COMMENT_END:
        return data
    mutable = bytearray(data)
    mutable[COMMENT_START:COMMENT_END] = b"\x00" * COMMENT_LEN
    return bytes(mutable)


def first_diff(a: bytes, b: bytes):
    limit = min(len(a), len(b))
    for i in range(limit):
        if a[i] != b[i]:
            return i, a[i], b[i]
    if len(a) != len(b):
        return limit, None, None
    return None, None, None


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: compare-8xp.py <file1.8xp> <file2.8xp>")
        return 2

    p1 = Path(sys.argv[1])
    p2 = Path(sys.argv[2])

    if not p1.exists() or not p2.exists():
        print("Both files must exist.")
        return 2

    data1 = p1.read_bytes()
    data2 = p2.read_bytes()

    masked1 = mask_comment(data1)
    masked2 = mask_comment(data2)

    if masked1 == masked2:
        print("MATCH (comment bytes ignored)")
        return 0

    idx, b1, b2 = first_diff(masked1, masked2)
    if b1 is None and b2 is None:
        print("MISMATCH: file lengths differ after masking comment bytes")
    else:
        print(f"MISMATCH at offset {idx}: 0x{b1:02x} vs 0x{b2:02x}")

    print(f"len1={len(masked1)} len2={len(masked2)}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
