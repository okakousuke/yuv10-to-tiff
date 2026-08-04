#!/usr/bin/env python3
"""Create a small synthetic packed-10bit YCbCr 4:2:2 sample."""

from pathlib import Path
import sys


def pack(values: list[int]) -> bytes:
    if len(values) % 3:
        raise ValueError("values must be a multiple of 3")
    result = bytearray()
    for start in range(0, len(values), 3):
        word = (values[start] << 22) | (values[start + 1] << 12) | (values[start + 2] << 2)
        result.extend(word.to_bytes(4, "big"))
    return bytes(result)


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} OUTPUT WIDTH HEIGHT", file=sys.stderr)
        return 2
    output = Path(sys.argv[1])
    width = int(sys.argv[2])
    height = int(sys.argv[3])
    if width <= 0 or height <= 0 or width % 2:
        raise ValueError("width and height must be positive; width must be even")

    values: list[int] = []
    for _ in range(width * height // 2):
        values.extend((512, 512, 512, 512))
    output.write_bytes(pack(values))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
